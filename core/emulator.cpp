/**
 * @file emulator.cpp
 * @brief Main emulator implementation with instruction execution loop.
 */

#include "emulator.h"
#include "../execution/decoder.h"
#include "../execution/alu.h"

namespace e2emu {

Emulator::Emulator()
    : m_instruction_count(0)
{
    m_vga = std::make_shared<VGADevice>(&m_memory);

    // Register VGA ports (CRT Controller)
    m_port_controller.registerDevice(0x3D4, 0x3D5, m_vga);
    m_port_controller.registerDevice(0x3DA, 0x3DA, m_vga);  // Input Status Register

    // Set up port controller callback to forward to emulator callbacks
    m_port_controller.setPortAccessCallback([this](uint16 port, uint8 value, bool is_input) {
        if (m_callbacks.onPortAccess) {
            m_callbacks.onPortAccess(port, value, is_input);
        }
    });

    // Set up port input request callback (called when IN instruction needs user input)
    m_port_controller.setPortInputRequestCallback([this](uint16 port) -> uint8 {
        if (m_callbacks.onPortInputRequest) {
            return m_callbacks.onPortInputRequest(port);
        }
        return 0xFF;  // Default if no callback set
    });

    m_interrupt_controller = std::make_unique<InterruptController>(&m_cpu, &m_memory, m_vga.get());

    // Note: Reset do the HW initialization so no need to have init() that will
    // do nothing but calling reset()
    reset();
}

Emulator::~Emulator() = default;

void Emulator::reset() {
    m_cpu.reset();
    m_memory.clear();
    m_instruction_count = 0;
    m_error.clear();

    // Clear cached port input values (so IN instruction asks for input again)
    m_port_controller.clearCachedValues();

    // Initialize VGA to text mode
    m_vga->setVideoMode(VGADevice::VideoMode::TEXT_80x25_16COLOR);

    // Initialize BIOS Data Area (segment 40h)
    // 40h:49h = current video mode
    m_memory.writeByte(0x449, 0x03);
    // 40h:4Ah = number of screen columns (word)
    m_memory.writeByte(0x44A, VGADevice::TEXT_WIDTH);
    m_memory.writeByte(0x44B, 0x00);
    // 40h:4Ch = size of current video page in bytes (word)
    m_memory.writeByte(0x44C, static_cast<uint8>(VGADevice::TEXT_BUFFER_SIZE & 0xFF));
    m_memory.writeByte(0x44D, static_cast<uint8>(VGADevice::TEXT_BUFFER_SIZE >> 8));
    // 40h:62h = current active display page
    m_memory.writeByte(0x462, 0x00);
    // 40h:84h = number of rows minus 1
    m_memory.writeByte(0x484, VGADevice::TEXT_HEIGHT - 1);

    notifyStateChanged(ExecutionState::HALTED);
}

void Emulator::loadBinary(const std::vector<uint8>& binary, PhysicalAddress start_addr) {
    m_memory.loadBinary(binary, start_addr);
}

void Emulator::loadProgram(const std::vector<uint8>& binary, uint16 code_segment, uint16 instruction_pointer) {
    // Calculate physical address where the binary will live
    PhysicalAddress start_addr = m_cpu.calculatePhysicalAddress(code_segment, instruction_pointer);

    loadBinary(binary, start_addr);

    // Initialize segment registers for COM-style programs
    // For COM programs, CS=DS=ES=SS all point to the same segment
    m_cpu.CS = code_segment;
    m_cpu.DS = code_segment;
    m_cpu.ES = code_segment;
    m_cpu.SS = code_segment;

    m_cpu.IP = instruction_pointer;

    // Initialize stack pointer to top of 64KB segment (typical for COM programs)
    m_cpu.SP = 0xFFFE;

    m_cpu.state = ExecutionState::RUNNING;

    notifyStateChanged(ExecutionState::RUNNING);
    notifyRegistersChanged();
}

void Emulator::halt() {
    m_cpu.state = ExecutionState::HALTED;
    notifyStateChanged(ExecutionState::HALTED);
}

bool Emulator::step() {
    if (m_cpu.state != ExecutionState::RUNNING) {
        return false;
    }

    if (!executeInstruction()) {
        return false;
    }

    m_instruction_count++;
    notifyInstructionExecuted();
    notifyRegistersChanged();

    return m_cpu.state == ExecutionState::RUNNING;
}

size_t Emulator::run(size_t max_instructions) {
    size_t count = 0;

    while (m_cpu.state == ExecutionState::RUNNING) {
        if (!step()) {
            break;
        }

        count++;

        if (max_instructions > 0 && count >= max_instructions) {
            break;
        }
    }

    return count;
}

bool Emulator::executeInstruction() {
    // Fetch instruction byte
    PhysicalAddress ip_addr = m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP);
    uint8 opcode = m_memory.readByte(ip_addr);

    // Check for segment override prefixes
    SegReg seg_override = SegReg::DS;  // Default segment
    bool has_seg_override = false;

    if (opcode == 0x26 || opcode == 0x2E || opcode == 0x36 || opcode == 0x3E) {
        // Segment override prefix
        has_seg_override = true;
        switch (opcode) {
            case 0x26: seg_override = SegReg::ES; break;
            case 0x2E: seg_override = SegReg::CS; break;
            case 0x36: seg_override = SegReg::SS; break;
            case 0x3E: seg_override = SegReg::DS; break;
        }

        // Advance IP and fetch the actual instruction opcode
        m_cpu.IP++;
        ip_addr = m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP);
        opcode = m_memory.readByte(ip_addr);
    }

    // Decode and execute

    switch (opcode) {
        case 0x90:  // NOP
            m_cpu.IP++;
            return true;

        case 0xF4:  // HLT
            m_cpu.IP++;
            halt();
            return false;

        case 0xCD:  // INT imm8
        {
            m_cpu.IP++;
            uint8 interrupt_num = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            // Handle interrupt
            if (!m_interrupt_controller->handleInterrupt(interrupt_num)) {
                // Interrupt requested halt
                return false;
            }
            return true;
        }

        case 0xB0:  // MOV AL, imm8
        case 0xB1:  // MOV CL, imm8
        case 0xB2:  // MOV DL, imm8
        case 0xB3:  // MOV BL, imm8
        case 0xB4:  // MOV AH, imm8
        case 0xB5:  // MOV CH, imm8
        case 0xB6:  // MOV DH, imm8
        case 0xB7:  // MOV BH, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            Reg8 reg = static_cast<Reg8>(opcode - 0xB0);
            m_cpu.setReg8(reg, imm);
            return true;
        }

        case 0xB8:  // MOV AX, imm16
        case 0xB9:  // MOV CX, imm16
        case 0xBA:  // MOV DX, imm16
        case 0xBB:  // MOV BX, imm16
        case 0xBC:  // MOV SP, imm16
        case 0xBD:  // MOV BP, imm16
        case 0xBE:  // MOV SI, imm16
        case 0xBF:  // MOV DI, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            Reg16 reg = static_cast<Reg16>(opcode - 0xB8);
            m_cpu.setReg16(reg, imm);
            return true;
        }

        case 0x88:  // MOV r/m8, r8
        case 0x89:  // MOV r/m16, r16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            uint16 value = decoder.readOperand(src, is_word);
            decoder.writeOperand(dst, value, is_word);
            return true;
        }

        case 0x8A:  // MOV r8, r/m8
        case 0x8B:  // MOV r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            // For MOV r, r/m, the direction is reversed
            uint16 value = decoder.readOperand(dst, is_word);
            decoder.writeOperand(src, value, is_word);
            return true;
        }

        case 0xC6:  // MOV r/m8, imm8
        case 0xC7:  // MOV r/m16, imm16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            // Decode destination operand
            Operand dst, unused;
            decoder.decodeModRM(modrm, is_word, dst, unused);

            // Read immediate value
            uint16 imm;
            if (is_word) {
                imm = decoder.fetchWord();
            } else {
                imm = decoder.fetchByte();
            }

            // Write immediate to destination
            decoder.writeOperand(dst, imm, is_word);
            return true;
        }

        case 0x04:  // ADD AL, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            uint8 result = alu.add(m_cpu.getReg8(Reg8::AL), imm, false);
            m_cpu.setReg8(Reg8::AL, result);
            return true;
        }

        case 0x75:  // JNZ rel8
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;

            if (!m_cpu.flags.ZF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0xF6:  // GRP3 r/m8 (TEST, NOT, NEG, MUL, IMUL, DIV, IDIV)
        {
            m_cpu.IP++;

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand operand, unused;
            decoder.decodeModRM(modrm, false, operand, unused);

            ALU alu(&m_cpu);

            switch (modrm.reg) {
                case 0:  // TEST r/m8, imm8
                {
                    uint8 imm = decoder.fetchByte();
                    uint8 value = decoder.readOperand(operand, false);
                    alu.test(value, imm, false);
                    break;
                }
                case 2:  // NOT r/m8
                {
                    uint8 value = decoder.readOperand(operand, false);
                    uint8 result = alu.not_op(value, false);
                    decoder.writeOperand(operand, result, false);
                    break;
                }
                case 3:  // NEG r/m8
                {
                    uint8 value = decoder.readOperand(operand, false);
                    uint8 result = alu.neg(value, false);
                    decoder.writeOperand(operand, result, false);
                    break;
                }
                case 4:  // MUL r/m8
                {
                    uint8 src = decoder.readOperand(operand, false);
                    alu.mul(src, false, false);  // Result is stored directly in AX
                    break;
                }
                case 5:  // IMUL r/m8
                {
                    uint8 src = decoder.readOperand(operand, false);
                    alu.mul(src, false, true);  // Result is stored directly in AX
                    break;
                }
                case 6:  // DIV r/m8
                {
                    uint8 divisor = decoder.readOperand(operand, false);
                    uint16 dividend = m_cpu.AX;
                    uint16 remainder;
                    uint16 quotient = alu.div(0, dividend, divisor, false, false, remainder);
                    m_cpu.setReg8(Reg8::AL, quotient);
                    m_cpu.setReg8(Reg8::AH, remainder);
                    break;
                }
                case 7:  // IDIV r/m8
                {
                    uint8 divisor = decoder.readOperand(operand, false);
                    uint16 dividend = m_cpu.AX;
                    uint16 remainder;
                    uint16 quotient = alu.div(0, dividend, divisor, false, true, remainder);
                    m_cpu.setReg8(Reg8::AL, quotient);
                    m_cpu.setReg8(Reg8::AH, remainder);
                    break;
                }
                default:
                    setError("Invalid F6 subopcode");
                    return false;
            }
            return true;
        }

        case 0xFE:  // GRP4 r/m8 (INC, DEC)
        {
            m_cpu.IP++;

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand operand, unused;
            decoder.decodeModRM(modrm, false, operand, unused);

            ALU alu(&m_cpu);
            uint8 value = decoder.readOperand(operand, false);

            switch (modrm.reg) {
                case 0:  // INC r/m8
                {
                    uint8 result = alu.inc(value, false);
                    decoder.writeOperand(operand, result, false);
                    break;
                }
                case 1:  // DEC r/m8
                {
                    uint8 result = alu.dec(value, false);
                    decoder.writeOperand(operand, result, false);
                    break;
                }
                default:
                    setError("Invalid FE subopcode");
                    return false;
            }
            return true;
        }

        case 0x60:  // PUSHA
        {
            m_cpu.IP++;
            uint16 temp_sp = m_cpu.SP;

            // Push in order: AX, CX, DX, BX, original SP, BP, SI, DI
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.AX);
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.CX);
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.DX);
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.BX);
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), temp_sp);
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.BP);
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.SI);
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.DI);

            return true;
        }

        case 0x61:  // POPA
        {
            m_cpu.IP++;

            // Pop in reverse order: DI, SI, BP, (skip SP), BX, DX, CX, AX
            m_cpu.DI = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            m_cpu.SI = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            m_cpu.BP = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            // Skip SP value for POPA
            m_cpu.SP += 2;
            m_cpu.BX = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            m_cpu.DX = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            m_cpu.CX = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            m_cpu.AX = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;

            return true;
        }

        // ========== Flag instructions ==========
        case 0xFA:  // CLI - Clear interrupt flag
            m_cpu.IP++;
            m_cpu.flags.IF = false;
            return true;

        case 0xFB:  // STI - Set interrupt flag
            m_cpu.IP++;
            m_cpu.flags.IF = true;
            return true;

        case 0xFC:  // CLD - Clear direction flag
            m_cpu.IP++;
            m_cpu.flags.DF = false;
            return true;

        case 0xFD:  // STD - Set direction flag
            m_cpu.IP++;
            m_cpu.flags.DF = true;
            return true;

        case 0xF8:  // CLC - Clear carry flag
            m_cpu.IP++;
            m_cpu.flags.CF = false;
            return true;

        case 0xF9:  // STC - Set carry flag
            m_cpu.IP++;
            m_cpu.flags.CF = true;
            return true;

        case 0xF5:  // CMC - Complement carry flag
            m_cpu.IP++;
            m_cpu.flags.CF = !m_cpu.flags.CF;
            return true;

        // ========== BCD/ASCII adjust instructions ==========
        case 0x27:  // DAA - Decimal adjust after addition
        {
            m_cpu.IP++;
            ALU alu(&m_cpu);
            alu.daa();
            return true;
        }

        case 0x2F:  // DAS - Decimal adjust after subtraction
        {
            m_cpu.IP++;
            ALU alu(&m_cpu);
            alu.das();
            return true;
        }

        case 0x37:  // AAA - ASCII adjust after addition
        {
            m_cpu.IP++;
            ALU alu(&m_cpu);
            alu.aaa();
            return true;
        }

        case 0x3F:  // AAS - ASCII adjust after subtraction
        {
            m_cpu.IP++;
            ALU alu(&m_cpu);
            alu.aas();
            return true;
        }

        case 0xD4:  // AAM - ASCII adjust after multiplication
        {
            m_cpu.IP++;
            // AAM has an immediate byte (base), usually 0x0A for decimal
            uint8 base = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            if (base == 0) {
                setError("AAM: division by zero");
                return false;
            }

            ALU alu(&m_cpu);
            alu.aam(base);
            return true;
        }

        case 0xD5:  // AAD - ASCII adjust before division
        {
            m_cpu.IP++;
            // AAD has an immediate byte (base), usually 0x0A for decimal
            uint8 base = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            alu.aad(base);
            return true;
        }

        case 0x9F:  // LAHF - Load AH from flags
            m_cpu.IP++;
            m_cpu.setReg8(Reg8::AH, m_cpu.flags.toWord() & 0xFF);
            return true;

        case 0x9E:  // SAHF - Store AH to flags
            m_cpu.IP++;
            m_cpu.flags.fromWord((m_cpu.flags.toWord() & 0xFF00) | m_cpu.getReg8(Reg8::AH));
            return true;

        case 0x9C:  // PUSHF - Push flags
            m_cpu.IP++;
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.flags.toWord());
            return true;

        case 0x9D:  // POPF - Pop flags
            m_cpu.IP++;
            m_cpu.flags.fromWord(m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP)));
            m_cpu.SP += 2;
            return true;

        case 0x99:  // CWD - Convert word to doubleword
            m_cpu.IP++;
            // Sign-extend AX into DX:AX
            if (m_cpu.AX & 0x8000) {
                m_cpu.DX = 0xFFFF;
            } else {
                m_cpu.DX = 0x0000;
            }
            return true;

        // ========== PUSH/POP reg16 ==========
        case 0x50: case 0x51: case 0x52: case 0x53:  // PUSH AX/CX/DX/BX
        case 0x54: case 0x55: case 0x56: case 0x57:  // PUSH SP/BP/SI/DI
        {
            m_cpu.IP++;
            Reg16 reg = static_cast<Reg16>(opcode - 0x50);
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.getReg16(reg));
            return true;
        }

        case 0x58: case 0x59: case 0x5A: case 0x5B:  // POP AX/CX/DX/BX
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:  // POP SP/BP/SI/DI
        {
            m_cpu.IP++;
            Reg16 reg = static_cast<Reg16>(opcode - 0x58);
            m_cpu.setReg16(reg, m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP)));
            m_cpu.SP += 2;
            return true;
        }

        // ========== PUSH/POP segment registers ==========
        case 0x06:  // PUSH ES
            m_cpu.IP++;
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.ES);
            return true;

        case 0x0E:  // PUSH CS
            m_cpu.IP++;
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.CS);
            return true;

        case 0x16:  // PUSH SS
            m_cpu.IP++;
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.SS);
            return true;

        case 0x1E:  // PUSH DS
            m_cpu.IP++;
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.DS);
            return true;

        case 0x07:  // POP ES
            m_cpu.IP++;
            m_cpu.ES = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            return true;

        case 0x0F:  // POP CS (never seen it used but, anyways, lets support it)
            m_cpu.IP++;
            m_cpu.CS = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            return true;

        case 0x17:  // POP SS
            m_cpu.IP++;
            m_cpu.SS = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            return true;

        case 0x1F:  // POP DS
            m_cpu.IP++;
            m_cpu.DS = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            return true;

        // ========== INC/DEC reg16 ==========
        case 0x40: case 0x41: case 0x42: case 0x43:  // INC AX/CX/DX/BX
        case 0x44: case 0x45: case 0x46: case 0x47:  // INC SP/BP/SI/DI
        {
            m_cpu.IP++;
            Reg16 reg = static_cast<Reg16>(opcode - 0x40);
            ALU alu(&m_cpu);
            uint16 result = alu.inc(m_cpu.getReg16(reg), true);
            m_cpu.setReg16(reg, result);
            return true;
        }

        case 0x48: case 0x49: case 0x4A: case 0x4B:  // DEC AX/CX/DX/BX
        case 0x4C: case 0x4D: case 0x4E: case 0x4F:  // DEC SP/BP/SI/DI
        {
            m_cpu.IP++;
            Reg16 reg = static_cast<Reg16>(opcode - 0x48);
            ALU alu(&m_cpu);
            uint16 result = alu.dec(m_cpu.getReg16(reg), true);
            m_cpu.setReg16(reg, result);
            return true;
        }

        // ========== XCHG ==========
        case 0x91: case 0x92: case 0x93:  // XCHG AX, CX/DX/BX
        case 0x94: case 0x95: case 0x96: case 0x97:  // XCHG AX, SP/BP/SI/DI
        {
            m_cpu.IP++;
            Reg16 reg = static_cast<Reg16>(opcode - 0x90);
            uint16 temp = m_cpu.AX;
            m_cpu.AX = m_cpu.getReg16(reg);
            m_cpu.setReg16(reg, temp);
            return true;
        }

        case 0x86:  // XCHG r/m8, r8
        case 0x87:  // XCHG r/m16, r16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand op1, op2;
            decoder.decodeModRM(modrm, is_word, op1, op2);

            uint16 val1 = decoder.readOperand(op1, is_word);
            uint16 val2 = decoder.readOperand(op2, is_word);

            decoder.writeOperand(op1, val2, is_word);
            decoder.writeOperand(op2, val1, is_word);
            return true;
        }

        // ========== ADD with ModRM ==========
        case 0x00:  // ADD r/m8, r8
        case 0x01:  // ADD r/m16, r16
        case 0x02:  // ADD r8, r/m8
        case 0x03:  // ADD r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool reg_is_dest = (opcode & 2);  // Direction bit

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            if (reg_is_dest) {
                std::swap(dst, src);  // Reverse direction
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(dst, is_word);
            uint16 src_val = decoder.readOperand(src, is_word);
            uint16 result = alu.add(dst_val, src_val, is_word, false);
            decoder.writeOperand(dst, result, is_word);
            return true;
        }

        case 0x05:  // ADD AX, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            ALU alu(&m_cpu);
            uint16 result = alu.add(m_cpu.AX, imm, true, false);
            m_cpu.AX = result;
            return true;
        }

        // ========== ADC (Add with Carry) ==========
        case 0x10:  // ADC r/m8, r8
        case 0x11:  // ADC r/m16, r16
        case 0x12:  // ADC r8, r/m8
        case 0x13:  // ADC r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool reg_is_dest = (opcode & 2);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            if (reg_is_dest) {
                std::swap(dst, src);
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(dst, is_word);
            uint16 src_val = decoder.readOperand(src, is_word);
            uint16 result = alu.add(dst_val, src_val, is_word, true);  // with_carry = true
            decoder.writeOperand(dst, result, is_word);
            return true;
        }

        case 0x14:  // ADC AL, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            uint8 result = alu.add(m_cpu.getReg8(Reg8::AL), imm, false, true);
            m_cpu.setReg8(Reg8::AL, result);
            return true;
        }

        case 0x15:  // ADC AX, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            ALU alu(&m_cpu);
            uint16 result = alu.add(m_cpu.AX, imm, true, true);
            m_cpu.AX = result;
            return true;
        }

        // ========== SUB ==========
        case 0x28:  // SUB r/m8, r8
        case 0x29:  // SUB r/m16, r16
        case 0x2A:  // SUB r8, r/m8
        case 0x2B:  // SUB r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool reg_is_dest = (opcode & 2);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            if (reg_is_dest) {
                std::swap(dst, src);
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(dst, is_word);
            uint16 src_val = decoder.readOperand(src, is_word);
            uint16 result = alu.sub(dst_val, src_val, is_word, false);
            decoder.writeOperand(dst, result, is_word);
            return true;
        }

        case 0x2C:  // SUB AL, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            uint8 result = alu.sub(m_cpu.getReg8(Reg8::AL), imm, false, false);
            m_cpu.setReg8(Reg8::AL, result);
            return true;
        }

        case 0x2D:  // SUB AX, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            ALU alu(&m_cpu);
            uint16 result = alu.sub(m_cpu.AX, imm, true, false);
            m_cpu.AX = result;
            return true;
        }

        // ========== SBB (Subtract with Borrow) ==========
        case 0x18:  // SBB r/m8, r8
        case 0x19:  // SBB r/m16, r16
        case 0x1A:  // SBB r8, r/m8
        case 0x1B:  // SBB r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool reg_is_dest = (opcode & 2);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            if (reg_is_dest) {
                std::swap(dst, src);
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(dst, is_word);
            uint16 src_val = decoder.readOperand(src, is_word);
            uint16 result = alu.sub(dst_val, src_val, is_word, true);  // with_borrow = true
            decoder.writeOperand(dst, result, is_word);
            return true;
        }

        case 0x1C:  // SBB AL, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            uint8 result = alu.sub(m_cpu.getReg8(Reg8::AL), imm, false, true);
            m_cpu.setReg8(Reg8::AL, result);
            return true;
        }

        case 0x1D:  // SBB AX, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            ALU alu(&m_cpu);
            uint16 result = alu.sub(m_cpu.AX, imm, true, true);
            m_cpu.AX = result;
            return true;
        }

        // ========== CMP ==========
        case 0x38:  // CMP r/m8, r8
        case 0x39:  // CMP r/m16, r16
        case 0x3A:  // CMP r8, r/m8
        case 0x3B:  // CMP r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool reg_is_dest = (opcode & 2);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            if (reg_is_dest) {
                std::swap(dst, src);
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(dst, is_word);
            uint16 src_val = decoder.readOperand(src, is_word);
            alu.cmp(dst_val, src_val, is_word);
            return true;
        }

        case 0x3C:  // CMP AL, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            alu.cmp(m_cpu.getReg8(Reg8::AL), imm, false);
            return true;
        }

        case 0x3D:  // CMP AX, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            ALU alu(&m_cpu);
            alu.cmp(m_cpu.AX, imm, true);
            return true;
        }

        // ========== AND ==========
        case 0x20:  // AND r/m8, r8
        case 0x21:  // AND r/m16, r16
        case 0x22:  // AND r8, r/m8
        case 0x23:  // AND r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool reg_is_dest = (opcode & 2);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            if (reg_is_dest) {
                std::swap(dst, src);
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(dst, is_word);
            uint16 src_val = decoder.readOperand(src, is_word);
            uint16 result = alu.and_op(dst_val, src_val, is_word);
            decoder.writeOperand(dst, result, is_word);
            return true;
        }

        case 0x24:  // AND AL, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            uint8 result = alu.and_op(m_cpu.getReg8(Reg8::AL), imm, false);
            m_cpu.setReg8(Reg8::AL, result);
            return true;
        }

        case 0x25:  // AND AX, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            ALU alu(&m_cpu);
            uint16 result = alu.and_op(m_cpu.AX, imm, true);
            m_cpu.AX = result;
            return true;
        }

        // ========== OR ==========
        case 0x08:  // OR r/m8, r8
        case 0x09:  // OR r/m16, r16
        case 0x0A:  // OR r8, r/m8
        case 0x0B:  // OR r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool reg_is_dest = (opcode & 2);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            if (reg_is_dest) {
                std::swap(dst, src);
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(dst, is_word);
            uint16 src_val = decoder.readOperand(src, is_word);
            uint16 result = alu.or_op(dst_val, src_val, is_word);
            decoder.writeOperand(dst, result, is_word);
            return true;
        }

        case 0x0C:  // OR AL, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            uint8 result = alu.or_op(m_cpu.getReg8(Reg8::AL), imm, false);
            m_cpu.setReg8(Reg8::AL, result);
            return true;
        }

        case 0x0D:  // OR AX, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            ALU alu(&m_cpu);
            uint16 result = alu.or_op(m_cpu.AX, imm, true);
            m_cpu.AX = result;
            return true;
        }

        // ========== XOR ==========
        case 0x30:  // XOR r/m8, r8
        case 0x31:  // XOR r/m16, r16
        case 0x32:  // XOR r8, r/m8
        case 0x33:  // XOR r16, r/m16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool reg_is_dest = (opcode & 2);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, src;
            decoder.decodeModRM(modrm, is_word, dst, src);

            if (reg_is_dest) {
                std::swap(dst, src);
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(dst, is_word);
            uint16 src_val = decoder.readOperand(src, is_word);
            uint16 result = alu.xor_op(dst_val, src_val, is_word);
            decoder.writeOperand(dst, result, is_word);
            return true;
        }

        case 0x34:  // XOR AL, imm8
        {
            m_cpu.IP++;
            uint8 imm = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            ALU alu(&m_cpu);
            uint8 result = alu.xor_op(m_cpu.getReg8(Reg8::AL), imm, false);
            m_cpu.setReg8(Reg8::AL, result);
            return true;
        }

        case 0x35:  // XOR AX, imm16
        {
            m_cpu.IP++;
            uint16 imm = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            ALU alu(&m_cpu);
            uint16 result = alu.xor_op(m_cpu.AX, imm, true);
            m_cpu.AX = result;
            return true;
        }
        // ========== GRP1 (0x80-0x83): ALU operations with immediate ==========
        case 0x80:  // GRP1 r/m8, imm8
        case 0x81:  // GRP1 r/m16, imm16
        case 0x82:  // GRP1 r/m8, imm8 (alias of 0x80)
        case 0x83:  // GRP1 r/m16, sign-extended imm8
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1) || (opcode == 0x83);
            bool sign_extend = (opcode == 0x83);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand operand, unused;
            decoder.decodeModRM(modrm, is_word, operand, unused);

            uint16 imm;
            if (sign_extend || !is_word) {
                int8_t imm8 = static_cast<int8_t>(decoder.fetchByte());
                imm = sign_extend ? static_cast<int16_t>(imm8) : static_cast<uint8_t>(imm8);
            } else {
                imm = decoder.fetchWord();
            }

            ALU alu(&m_cpu);
            uint16 dst_val = decoder.readOperand(operand, is_word);
            uint16 result = 0;

            switch (modrm.reg) {
                case 0:  // ADD
                    result = alu.add(dst_val, imm, is_word, false);
                    decoder.writeOperand(operand, result, is_word);
                    break;
                case 1:  // OR
                    result = alu.or_op(dst_val, imm, is_word);
                    decoder.writeOperand(operand, result, is_word);
                    break;
                case 2:  // ADC
                    result = alu.add(dst_val, imm, is_word, true);
                    decoder.writeOperand(operand, result, is_word);
                    break;
                case 3:  // SBB
                    result = alu.sub(dst_val, imm, is_word, true);
                    decoder.writeOperand(operand, result, is_word);
                    break;
                case 4:  // AND
                    result = alu.and_op(dst_val, imm, is_word);
                    decoder.writeOperand(operand, result, is_word);
                    break;
                case 5:  // SUB
                    result = alu.sub(dst_val, imm, is_word, false);
                    decoder.writeOperand(operand, result, is_word);
                    break;
                case 6:  // XOR
                    result = alu.xor_op(dst_val, imm, is_word);
                    decoder.writeOperand(operand, result, is_word);
                    break;
                case 7:  // CMP
                    alu.cmp(dst_val, imm, is_word);
                    break;
                default:
                    setError("Invalid GRP1 subopcode");
                    return false;
            }
            return true;
        }

        // ========== TEST r/m, r ==========
        case 0x84:  // TEST r/m8, r8
        case 0x85:  // TEST r/m16, r16
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand op1, op2;
            decoder.decodeModRM(modrm, is_word, op1, op2);

            ALU alu(&m_cpu);
            uint16 val1 = decoder.readOperand(op1, is_word);
            uint16 val2 = decoder.readOperand(op2, is_word);
            alu.test(val1, val2, is_word);
            return true;
        }

        // ========== MOV r/m16, segreg ==========
        case 0x8C:  // MOV r/m16, segreg
        {
            m_cpu.IP++;

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand dst, unused;
            decoder.decodeModRM(modrm, true, dst, unused);

            uint16 seg_value = m_cpu.seg_regs[modrm.reg];
            decoder.writeOperand(dst, seg_value, true);
            return true;
        }

        // ========== LEA r16, m ==========
        case 0x8D:  // LEA r16, m
        {
            m_cpu.IP++;

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            // For LEA, we need to calculate the effective address (offset) manually
            // without loading from memory
            uint16 offset = 0;

            if (modrm.mod == 3) {
                setError("LEA requires memory operand, not register");
                return false;
            }

            if (modrm.mod == 0 && modrm.rm == 6) {
                // Direct addressing: [disp16]
                offset = decoder.fetchWord();
            } else {
                // Calculate base from R/M field
                switch (modrm.rm) {
                    case 0: offset = m_cpu.BX + m_cpu.SI; break;  // [BX+SI]
                    case 1: offset = m_cpu.BX + m_cpu.DI; break;  // [BX+DI]
                    case 2: offset = m_cpu.BP + m_cpu.SI; break;  // [BP+SI]
                    case 3: offset = m_cpu.BP + m_cpu.DI; break;  // [BP+DI]
                    case 4: offset = m_cpu.SI; break;              // [SI]
                    case 5: offset = m_cpu.DI; break;              // [DI]
                    case 6: offset = m_cpu.BP; break;              // [BP]
                    case 7: offset = m_cpu.BX; break;              // [BX]
                }

                // Add displacement
                if (modrm.mod == 1) {
                    // 8-bit signed displacement
                    int8_t disp = static_cast<int8_t>(decoder.fetchByte());
                    offset += disp;
                } else if (modrm.mod == 2) {
                    // 16-bit displacement
                    offset += decoder.fetchWord();
                }
            }

            // Store the effective address (offset) in the destination register
            m_cpu.setReg16(static_cast<Reg16>(modrm.reg), offset);
            return true;
        }

        // ========== MOV segreg, r/m16 ==========
        case 0x8E:  // MOV segreg, r/m16
        {
            m_cpu.IP++;

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand src, unused;
            decoder.decodeModRM(modrm, true, src, unused);

            uint16 value = decoder.readOperand(src, true);
            m_cpu.seg_regs[modrm.reg] = value;
            return true;
        }

        // ========== MOV accumulator special forms ==========
        case 0xA0:  // MOV AL, [moffs8]
        {
            m_cpu.IP++;
            uint16 offset = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            // Use segment override if present, otherwise default to DS
            uint16 seg = has_seg_override ? m_cpu.seg_regs[static_cast<int>(seg_override)] : m_cpu.DS;
            PhysicalAddress addr = m_cpu.calculatePhysicalAddress(seg, offset);
            m_cpu.setReg8(Reg8::AL, m_memory.readByte(addr));
            return true;
        }

        case 0xA1:  // MOV AX, [moffs16]
        {
            m_cpu.IP++;
            uint16 offset = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            // Use segment override if present, otherwise default to DS
            uint16 seg = has_seg_override ? m_cpu.seg_regs[static_cast<int>(seg_override)] : m_cpu.DS;
            PhysicalAddress addr = m_cpu.calculatePhysicalAddress(seg, offset);
            m_cpu.AX = m_memory.readWord(addr);
            return true;
        }

        case 0xA2:  // MOV [moffs8], AL
        {
            m_cpu.IP++;
            uint16 offset = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            // Use segment override if present, otherwise default to DS
            uint16 seg = has_seg_override ? m_cpu.seg_regs[static_cast<int>(seg_override)] : m_cpu.DS;
            PhysicalAddress addr = m_cpu.calculatePhysicalAddress(seg, offset);
            m_memory.writeByte(addr, m_cpu.getReg8(Reg8::AL));
            return true;
        }

        case 0xA3:  // MOV [moffs16], AX
        {
            m_cpu.IP++;
            uint16 offset = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP += 2;

            // Use segment override if present, otherwise default to DS
            uint16 seg = has_seg_override ? m_cpu.seg_regs[static_cast<int>(seg_override)] : m_cpu.DS;
            PhysicalAddress addr = m_cpu.calculatePhysicalAddress(seg, offset);
            m_memory.writeWord(addr, m_cpu.AX);
            return true;
        }

        // ========== String instructions ==========
        case 0xA4:  // MOVSB
        {
            m_cpu.IP++;
            PhysicalAddress src_addr = m_cpu.calculatePhysicalAddress(m_cpu.DS, m_cpu.SI);
            PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

            m_memory.writeByte(dst_addr, m_memory.readByte(src_addr));

            if (m_cpu.flags.DF) {
                m_cpu.SI--;
                m_cpu.DI--;
            } else {
                m_cpu.SI++;
                m_cpu.DI++;
            }
            return true;
        }

        case 0xA5:  // MOVSW
        {
            m_cpu.IP++;
            PhysicalAddress src_addr = m_cpu.calculatePhysicalAddress(m_cpu.DS, m_cpu.SI);
            PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

            m_memory.writeWord(dst_addr, m_memory.readWord(src_addr));

            if (m_cpu.flags.DF) {
                m_cpu.SI -= 2;
                m_cpu.DI -= 2;
            } else {
                m_cpu.SI += 2;
                m_cpu.DI += 2;
            }
            return true;
        }

        case 0xA6:  // CMPSB
        {
            m_cpu.IP++;
            PhysicalAddress src_addr = m_cpu.calculatePhysicalAddress(m_cpu.DS, m_cpu.SI);
            PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

            ALU alu(&m_cpu);
            alu.cmp(m_memory.readByte(dst_addr), m_memory.readByte(src_addr), false);

            if (m_cpu.flags.DF) {
                m_cpu.SI--;
                m_cpu.DI--;
            } else {
                m_cpu.SI++;
                m_cpu.DI++;
            }
            return true;
        }

        case 0xA7:  // CMPSW
        {
            m_cpu.IP++;
            PhysicalAddress src_addr = m_cpu.calculatePhysicalAddress(m_cpu.DS, m_cpu.SI);
            PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

            ALU alu(&m_cpu);
            alu.cmp(m_memory.readWord(dst_addr), m_memory.readWord(src_addr), true);

            if (m_cpu.flags.DF) {
                m_cpu.SI -= 2;
                m_cpu.DI -= 2;
            } else {
                m_cpu.SI += 2;
                m_cpu.DI += 2;
            }
            return true;
        }

        case 0xAA:  // STOSB
        {
            m_cpu.IP++;
            PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

            m_memory.writeByte(dst_addr, m_cpu.getReg8(Reg8::AL));

            if (m_cpu.flags.DF) {
                m_cpu.DI--;
            } else {
                m_cpu.DI++;
            }
            return true;
        }

        case 0xAB:  // STOSW
        {
            m_cpu.IP++;
            PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

            m_memory.writeWord(dst_addr, m_cpu.AX);

            if (m_cpu.flags.DF) {
                m_cpu.DI -= 2;
            } else {
                m_cpu.DI += 2;
            }
            return true;
        }

        case 0xAC:  // LODSB
        {
            m_cpu.IP++;
            PhysicalAddress src_addr = m_cpu.calculatePhysicalAddress(m_cpu.DS, m_cpu.SI);

            m_cpu.setReg8(Reg8::AL, m_memory.readByte(src_addr));

            if (m_cpu.flags.DF) {
                m_cpu.SI--;
            } else {
                m_cpu.SI++;
            }
            return true;
        }

        case 0xAD:  // LODSW
        {
            m_cpu.IP++;
            PhysicalAddress src_addr = m_cpu.calculatePhysicalAddress(m_cpu.DS, m_cpu.SI);

            m_cpu.AX = m_memory.readWord(src_addr);

            if (m_cpu.flags.DF) {
                m_cpu.SI -= 2;
            } else {
                m_cpu.SI += 2;
            }
            return true;
        }

        case 0xAE:  // SCASB
        {
            m_cpu.IP++;
            PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

            ALU alu(&m_cpu);
            alu.cmp(m_cpu.getReg8(Reg8::AL), m_memory.readByte(dst_addr), false);

            if (m_cpu.flags.DF) {
                m_cpu.DI--;
            } else {
                m_cpu.DI++;
            }
            return true;
        }

        case 0xAF:  // SCASW
        {
            m_cpu.IP++;
            PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

            ALU alu(&m_cpu);
            alu.cmp(m_cpu.AX, m_memory.readWord(dst_addr), true);

            if (m_cpu.flags.DF) {
                m_cpu.DI -= 2;
            } else {
                m_cpu.DI += 2;
            }
            return true;
        }

        // ========== REP prefixes ==========
        case 0xF2:  // REPNE/REPNZ
        case 0xF3:  // REP/REPE/REPZ
        {
            m_cpu.IP++;
            bool repeat_while_equal = (opcode == 0xF3);

            // Fetch the string instruction opcode
            uint8 string_op = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));

            while (m_cpu.CX != 0) {
                // Execute the string instruction by recursively calling executeInstruction
                // but we need to do it manually here to avoid infinite recursion

                bool should_continue = true;

                switch (string_op) {
                    case 0xA4:  // MOVSB
                    case 0xA5:  // MOVSW
                    {
                        bool is_word = (string_op & 1);
                        PhysicalAddress src_addr = m_cpu.calculatePhysicalAddress(m_cpu.DS, m_cpu.SI);
                        PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

                        if (is_word) {
                            m_memory.writeWord(dst_addr, m_memory.readWord(src_addr));
                            if (m_cpu.flags.DF) {
                                m_cpu.SI -= 2;
                                m_cpu.DI -= 2;
                            } else {
                                m_cpu.SI += 2;
                                m_cpu.DI += 2;
                            }
                        } else {
                            m_memory.writeByte(dst_addr, m_memory.readByte(src_addr));
                            if (m_cpu.flags.DF) {
                                m_cpu.SI--;
                                m_cpu.DI--;
                            } else {
                                m_cpu.SI++;
                                m_cpu.DI++;
                            }
                        }
                        break;
                    }

                    case 0xA6:  // CMPSB
                    case 0xA7:  // CMPSW
                    {
                        bool is_word = (string_op & 1);
                        PhysicalAddress src_addr = m_cpu.calculatePhysicalAddress(m_cpu.DS, m_cpu.SI);
                        PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

                        ALU alu(&m_cpu);
                        if (is_word) {
                            alu.cmp(m_memory.readWord(dst_addr), m_memory.readWord(src_addr), true);
                            if (m_cpu.flags.DF) {
                                m_cpu.SI -= 2;
                                m_cpu.DI -= 2;
                            } else {
                                m_cpu.SI += 2;
                                m_cpu.DI += 2;
                            }
                        } else {
                            alu.cmp(m_memory.readByte(dst_addr), m_memory.readByte(src_addr), false);
                            if (m_cpu.flags.DF) {
                                m_cpu.SI--;
                                m_cpu.DI--;
                            } else {
                                m_cpu.SI++;
                                m_cpu.DI++;
                            }
                        }

                        // Check condition for REPE/REPNE
                        if (repeat_while_equal && !m_cpu.flags.ZF) should_continue = false;
                        if (!repeat_while_equal && m_cpu.flags.ZF) should_continue = false;
                        break;
                    }

                    case 0xAA:  // STOSB
                    case 0xAB:  // STOSW
                    {
                        bool is_word = (string_op & 1);
                        PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

                        if (is_word) {
                            m_memory.writeWord(dst_addr, m_cpu.AX);
                            if (m_cpu.flags.DF) {
                                m_cpu.DI -= 2;
                            } else {
                                m_cpu.DI += 2;
                            }
                        } else {
                            m_memory.writeByte(dst_addr, m_cpu.getReg8(Reg8::AL));
                            if (m_cpu.flags.DF) {
                                m_cpu.DI--;
                            } else {
                                m_cpu.DI++;
                            }
                        }
                        break;
                    }

                    case 0xAE:  // SCASB
                    case 0xAF:  // SCASW
                    {
                        bool is_word = (string_op & 1);
                        PhysicalAddress dst_addr = m_cpu.calculatePhysicalAddress(m_cpu.ES, m_cpu.DI);

                        ALU alu(&m_cpu);
                        if (is_word) {
                            alu.cmp(m_cpu.AX, m_memory.readWord(dst_addr), true);
                            if (m_cpu.flags.DF) {
                                m_cpu.DI -= 2;
                            } else {
                                m_cpu.DI += 2;
                            }
                        } else {
                            alu.cmp(m_cpu.getReg8(Reg8::AL), m_memory.readByte(dst_addr), false);
                            if (m_cpu.flags.DF) {
                                m_cpu.DI--;
                            } else {
                                m_cpu.DI++;
                            }
                        }

                        // Check condition for REPE/REPNE
                        if (repeat_while_equal && !m_cpu.flags.ZF) should_continue = false;
                        if (!repeat_while_equal && m_cpu.flags.ZF) should_continue = false;
                        break;
                    }

                    default:
                        setError("Invalid string instruction with REP prefix");
                        return false;
                }

                m_cpu.CX--;
                if (!should_continue || m_cpu.CX == 0) {
                    break;
                }
            }

            m_cpu.IP++;  // Skip the string instruction opcode
            return true;
        }

        // ========== Control flow ==========
        case 0xC3:  // RET (near return)
        {
            m_cpu.IP = m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP));
            m_cpu.SP += 2;
            return true;
        }

        case 0xE8:  // CALL rel16 (near call)
        {
            m_cpu.IP++;
            int16_t offset = static_cast<int16_t>(m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP += 2;

            // Push return address
            m_cpu.SP -= 2;
            m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.IP);

            // Jump to target
            m_cpu.IP += offset;
            return true;
        }

        case 0xE9:  // JMP near rel16
        {
            m_cpu.IP++;
            int16_t offset = static_cast<int16_t>(m_memory.readWord(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP += 2;
            m_cpu.IP += offset;
            return true;
        }

        case 0xEB:  // JMP short rel8
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            m_cpu.IP += offset;
            return true;
        }

        // ========== LOOP instructions ==========
        case 0xE0:  // LOOPNE/LOOPNZ rel8
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;

            m_cpu.CX--;
            if (m_cpu.CX != 0 && !m_cpu.flags.ZF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0xE1:  // LOOPE/LOOPZ rel8
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;

            m_cpu.CX--;
            if (m_cpu.CX != 0 && m_cpu.flags.ZF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0xE2:  // LOOP rel8
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;

            m_cpu.CX--;
            if (m_cpu.CX != 0) {
                m_cpu.IP += offset;
            }
            return true;
        }

        // ========== Conditional jumps ==========
        case 0x70:  // JO - Jump if overflow
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (m_cpu.flags.OF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x71:  // JNO - Jump if not overflow
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (!m_cpu.flags.OF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x72:  // JB/JC/JNAE - Jump if below/carry
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (m_cpu.flags.CF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x73:  // JNB/JAE/JNC - Jump if not below/above or equal/not carry
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (!m_cpu.flags.CF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x74:  // JE/JZ - Jump if equal/zero
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (m_cpu.flags.ZF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x76:  // JBE/JNA - Jump if below or equal/not above
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (m_cpu.flags.CF || m_cpu.flags.ZF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x77:  // JA/JNBE - Jump if above/not below or equal
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (!m_cpu.flags.CF && !m_cpu.flags.ZF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x78:  // JS - Jump if sign
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (m_cpu.flags.SF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x79:  // JNS - Jump if not sign
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (!m_cpu.flags.SF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x7C:  // JL/JNGE - Jump if less/not greater or equal
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (m_cpu.flags.SF != m_cpu.flags.OF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x7D:  // JNL/JGE - Jump if not less/greater or equal
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (m_cpu.flags.SF == m_cpu.flags.OF) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x7E:  // JLE/JNG - Jump if less or equal/not greater
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (m_cpu.flags.ZF || (m_cpu.flags.SF != m_cpu.flags.OF)) {
                m_cpu.IP += offset;
            }
            return true;
        }

        case 0x7F:  // JG/JNLE - Jump if greater/not less or equal
        {
            m_cpu.IP++;
            int8_t offset = static_cast<int8_t>(m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP)));
            m_cpu.IP++;
            if (!m_cpu.flags.ZF && (m_cpu.flags.SF == m_cpu.flags.OF)) {
                m_cpu.IP += offset;
            }
            return true;
        }

        // ========== GRP2: Shift/Rotate ==========
        case 0xD0:  // GRP2 r/m8, 1
        case 0xD1:  // GRP2 r/m16, 1
        case 0xD2:  // GRP2 r/m8, CL
        case 0xD3:  // GRP2 r/m16, CL
        {
            m_cpu.IP++;
            bool is_word = (opcode & 1);
            bool use_cl = (opcode >= 0xD2);

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand operand, unused;
            decoder.decodeModRM(modrm, is_word, operand, unused);

            uint8 count = use_cl ? m_cpu.getReg8(Reg8::CL) : 1;
            uint16 value = decoder.readOperand(operand, is_word);

            ALU alu(&m_cpu);
            uint16 result = 0;

            switch (modrm.reg) {
                case 0:  // ROL
                    result = alu.rol(value, count, is_word);
                    break;
                case 1:  // ROR
                    result = alu.ror(value, count, is_word);
                    break;
                case 2:  // RCL
                    result = alu.rcl(value, count, is_word);
                    break;
                case 3:  // RCR
                    result = alu.rcr(value, count, is_word);
                    break;
                case 4:  // SHL/SAL
                    result = alu.shl(value, count, is_word);
                    break;
                case 5:  // SHR
                    result = alu.shr(value, count, is_word);
                    break;
                case 7:  // SAR
                    result = alu.sar(value, count, is_word);
                    break;
                default:
                    setError("Invalid GRP2 subopcode");
                    return false;
            }

            decoder.writeOperand(operand, result, is_word);
            return true;
        }

        // ========== GRP3 (16-bit): TEST, NOT, NEG, MUL, IMUL, DIV, IDIV ==========
        case 0xF7:  // GRP3 r/m16
        {
            m_cpu.IP++;

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand operand, unused;
            decoder.decodeModRM(modrm, true, operand, unused);

            ALU alu(&m_cpu);

            switch (modrm.reg) {
                case 0:  // TEST r/m16, imm16
                {
                    uint16 imm = decoder.fetchWord();
                    uint16 value = decoder.readOperand(operand, true);
                    alu.test(value, imm, true);
                    break;
                }
                case 2:  // NOT r/m16
                {
                    uint16 value = decoder.readOperand(operand, true);
                    uint16 result = alu.not_op(value, true);
                    decoder.writeOperand(operand, result, true);
                    break;
                }
                case 3:  // NEG r/m16
                {
                    uint16 value = decoder.readOperand(operand, true);
                    uint16 result = alu.neg(value, true);
                    decoder.writeOperand(operand, result, true);
                    break;
                }
                case 4:  // MUL r/m16
                {
                    uint16 src = decoder.readOperand(operand, true);
                    alu.mul(src, true, false);  // Result is stored directly in DX:AX
                    break;
                }
                case 5:  // IMUL r/m16
                {
                    uint16 src = decoder.readOperand(operand, true);
                    alu.mul(src, true, true);  // Result is stored directly in DX:AX
                    break;
                }
                case 6:  // DIV r/m16
                {
                    uint16 divisor = decoder.readOperand(operand, true);
                    uint32 dividend = (static_cast<uint32>(m_cpu.DX) << 16) | m_cpu.AX;
                    uint16 remainder;
                    uint16 quotient = alu.div(m_cpu.DX, m_cpu.AX, divisor, true, false, remainder);
                    m_cpu.AX = quotient;
                    m_cpu.DX = remainder;
                    break;
                }
                case 7:  // IDIV r/m16
                {
                    uint16 divisor = decoder.readOperand(operand, true);
                    uint32 dividend = (static_cast<uint32>(m_cpu.DX) << 16) | m_cpu.AX;
                    uint16 remainder;
                    uint16 quotient = alu.div(m_cpu.DX, m_cpu.AX, divisor, true, true, remainder);
                    m_cpu.AX = quotient;
                    m_cpu.DX = remainder;
                    break;
                }
                default:
                    setError("Invalid F7 subopcode");
                    return false;
            }
            return true;
        }

        // ========== GRP5: INC/DEC/CALL/JMP/PUSH r/m16 ==========
        case 0xFF:  // GRP5 r/m16
        {
            m_cpu.IP++;

            Decoder decoder(&m_cpu, &m_memory);
            if (has_seg_override) decoder.setSegmentOverride(seg_override);
            uint8 modrm_byte = decoder.fetchByte();
            ModRM modrm(modrm_byte);

            Operand operand, unused;
            decoder.decodeModRM(modrm, true, operand, unused);

            ALU alu(&m_cpu);

            switch (modrm.reg) {
                case 0:  // INC r/m16
                {
                    uint16 value = decoder.readOperand(operand, true);
                    uint16 result = alu.inc(value, true);
                    decoder.writeOperand(operand, result, true);
                    break;
                }
                case 1:  // DEC r/m16
                {
                    uint16 value = decoder.readOperand(operand, true);
                    uint16 result = alu.dec(value, true);
                    decoder.writeOperand(operand, result, true);
                    break;
                }
                case 2:  // CALL r/m16 (indirect near call)
                {
                    uint16 target = decoder.readOperand(operand, true);
                    m_cpu.SP -= 2;
                    m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), m_cpu.IP);
                    m_cpu.IP = target;
                    break;
                }
                case 4:  // JMP r/m16 (indirect near jump)
                {
                    uint16 target = decoder.readOperand(operand, true);
                    m_cpu.IP = target;
                    break;
                }
                case 6:  // PUSH r/m16
                {
                    uint16 value = decoder.readOperand(operand, true);
                    m_cpu.SP -= 2;
                    m_memory.writeWord(m_cpu.calculatePhysicalAddress(m_cpu.SS, m_cpu.SP), value);
                    break;
                }
                default:
                    setError("Invalid FF subopcode");
                    return false;
            }
            return true;
        }

        // ========== IN/OUT Instructions ==========
        case 0xE4:  // IN AL, imm8
        {
            m_cpu.IP++;
            uint8 port = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            uint8 value = m_port_controller.in(port);
            m_cpu.setReg8(Reg8::AL, value);
            return true;
        }

        case 0xE5:  // IN AX, imm8
        {
            m_cpu.IP++;
            uint8 port = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            // Read word from port (8086 reads from port and port+1)
            uint8 low = m_port_controller.in(port);
            uint8 high = m_port_controller.in(port + 1);
            uint16 value = (static_cast<uint16>(high) << 8) | low;
            m_cpu.AX = value;
            return true;
        }

        case 0xE6:  // OUT imm8, AL
        {
            m_cpu.IP++;
            uint8 port = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            uint8 value = m_cpu.getReg8(Reg8::AL);
            m_port_controller.out(port, value);
            return true;
        }

        case 0xE7:  // OUT imm8, AX
        {
            m_cpu.IP++;
            uint8 port = m_memory.readByte(m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP));
            m_cpu.IP++;

            // Write word to port (8086 writes to port and port+1)
            uint16 value = m_cpu.AX;
            m_port_controller.out(port, static_cast<uint8>(value & 0xFF));
            m_port_controller.out(port + 1, static_cast<uint8>((value >> 8) & 0xFF));
            return true;
        }

        case 0xEC:  // IN AL, DX
        {
            m_cpu.IP++;
            uint16 port = m_cpu.DX;

            uint8 value = m_port_controller.in(port);
            m_cpu.setReg8(Reg8::AL, value);
            return true;
        }

        case 0xED:  // IN AX, DX
        {
            m_cpu.IP++;
            uint16 port = m_cpu.DX;

            // Read word from port (8086 reads from port and port+1)
            uint8 low = m_port_controller.in(port);
            uint8 high = m_port_controller.in(port + 1);
            uint16 value = (static_cast<uint16>(high) << 8) | low;
            m_cpu.AX = value;
            return true;
        }

        case 0xEE:  // OUT DX, AL
        {
            m_cpu.IP++;
            uint16 port = m_cpu.DX;

            uint8 value = m_cpu.getReg8(Reg8::AL);
            m_port_controller.out(port, value);
            return true;
        }

        case 0xEF:  // OUT DX, AX
        {
            m_cpu.IP++;
            uint16 port = m_cpu.DX;

            // Write word to port (8086 writes to port and port+1)
            uint16 value = m_cpu.AX;
            m_port_controller.out(port, static_cast<uint8>(value & 0xFF));
            m_port_controller.out(port + 1, static_cast<uint8>((value >> 8) & 0xFF));
            return true;
        }

        default:
        {
            // Unimplemented instruction
            char buf[100];
            snprintf(buf, sizeof(buf), "Unimplemented instruction: 0x%02X at CS:IP=%04X:%04X",
                     opcode, m_cpu.CS, m_cpu.IP);
            setError(buf);
            return false;
        }
    }
}

void Emulator::queueKeypress(uint8 scancode, uint8 ascii) {
    // Forward the pressed key to interrupt controller's keyboard buffer
    m_interrupt_controller->injectKeystroke(ascii, scancode);
}

std::string Emulator::getCurrentInstruction() const {
    PhysicalAddress ip_addr = m_cpu.calculatePhysicalAddress(m_cpu.CS, m_cpu.IP);
    uint8 opcode = m_memory.readByte(ip_addr);

    char buf[50];
    snprintf(buf, sizeof(buf), "%04X:%04X: %02X", m_cpu.CS, m_cpu.IP, opcode);
    return buf;
}

void Emulator::setError(const std::string& error) {
    m_error = error;
    m_cpu.state = ExecutionState::ERROR;
    notifyStateChanged(ExecutionState::ERROR);

    if (m_callbacks.onError) {
        m_callbacks.onError(error);
    }
}

void Emulator::notifyInstructionExecuted() {
    if (m_callbacks.onInstructionExecuted) {
        m_callbacks.onInstructionExecuted();
    }
}

void Emulator::notifyRegistersChanged() {
    if (m_callbacks.onRegistersChanged) {
        m_callbacks.onRegistersChanged();
    }
}

void Emulator::notifyStateChanged(ExecutionState new_state) {
    if (m_callbacks.onStateChanged) {
        m_callbacks.onStateChanged(new_state);
    }
}

} // namespace e2emu
