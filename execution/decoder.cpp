/**
 * @file decoder.cpp
 * @brief Instruction decoder implementation.
 */

#include "decoder.h"

namespace e2emu {

Decoder::Decoder(CPU* cpu, Memory* memory)
    : m_cpu(cpu)
    , m_memory(memory)
    , m_has_seg_override(false)
{
}

uint8 Decoder::fetchByte() {
    PhysicalAddress addr = m_cpu->calculatePhysicalAddress(m_cpu->CS, m_cpu->IP);
    uint8 value = m_memory->readByte(addr);
    m_cpu->IP++;
    return value;
}

uint16 Decoder::fetchWord() {
    PhysicalAddress addr = m_cpu->calculatePhysicalAddress(m_cpu->CS, m_cpu->IP);
    uint16 value = m_memory->readWord(addr);
    m_cpu->IP += 2;
    return value;
}

int Decoder::getDisplacementSize(uint8 mod, uint8 rm) {
    if (mod == 0 && rm == 6) {
        // Direct addressing: 16-bit displacement
        return 2;
    } else if (mod == 1) {
        // 8-bit displacement
        return 1;
    } else if (mod == 2) {
        // 16-bit displacement
        return 2;
    }
    return 0;  // No displacement
}

uint16 Decoder::calculateBaseAddress(uint8 rm) {
    switch (rm) {
        case 0: return m_cpu->BX + m_cpu->SI;  // [BX+SI]
        case 1: return m_cpu->BX + m_cpu->DI;  // [BX+DI]
        case 2: return m_cpu->BP + m_cpu->SI;  // [BP+SI]
        case 3: return m_cpu->BP + m_cpu->DI;  // [BP+DI]
        case 4: return m_cpu->SI;              // [SI]
        case 5: return m_cpu->DI;              // [DI]
        case 6: return m_cpu->BP;              // [BP] (special case in mod=0)
        case 7: return m_cpu->BX;              // [BX]
        default: return 0;
    }
}

PhysicalAddress Decoder::calculateEffectiveAddress(ModRM modrm, SegReg seg_override) {
    uint16 offset = 0;
    SegReg segment = m_has_seg_override ? m_seg_override : seg_override;

    if (modrm.mod == 3) {
        // Register mode - should not call this function
        return 0;
    }

    if (modrm.mod == 0 && modrm.rm == 6) {
        // Direct addressing: [disp16]
        offset = fetchWord();
    } else {
        // Calculate base from R/M field
        offset = calculateBaseAddress(modrm.rm);

        // Add displacement
        if (modrm.mod == 1) {
            // 8-bit signed displacement
            int8_t disp = static_cast<int8_t>(fetchByte());
            offset += disp;
        } else if (modrm.mod == 2) {
            // 16-bit displacement
            offset += fetchWord();
        }
    }

    // Use SS segment for BP-based addressing, DS otherwise
    if ((modrm.rm == 2 || modrm.rm == 3 || (modrm.rm == 6 && modrm.mod != 0)) && !m_has_seg_override) {
        segment = SegReg::SS;
    }

    return m_cpu->calculatePhysicalAddress(m_cpu->getSegReg(segment), offset);
}

void Decoder::decodeModRM(ModRM modrm, bool is_word, Operand& dst, Operand& src, SegReg default_seg) {
    // Destination operand (R/M field)
    if (modrm.mod == 3) {
        // Register mode
        if (is_word) {
            dst.type = Operand::REGISTER_16;
            dst.reg16 = static_cast<Reg16>(modrm.rm);
        } else {
            dst.type = Operand::REGISTER_8;
            dst.reg8 = static_cast<Reg8>(modrm.rm);
        }
    } else {
        // Memory mode
        dst.type = Operand::MEMORY;
        dst.mem_addr = calculateEffectiveAddress(modrm, default_seg);
    }

    // Source operand (REG field - always register)
    if (is_word) {
        src.type = Operand::REGISTER_16;
        src.reg16 = static_cast<Reg16>(modrm.reg);
    } else {
        src.type = Operand::REGISTER_8;
        src.reg8 = static_cast<Reg8>(modrm.reg);
    }

    clearSegmentOverride();
}

uint16 Decoder::readOperand(const Operand& op, bool is_word) {
    switch (op.type) {
        case Operand::REGISTER_8:
            return m_cpu->getReg8(op.reg8);

        case Operand::REGISTER_16:
            return m_cpu->getReg16(op.reg16);

        case Operand::MEMORY:
            if (is_word) {
                return m_memory->readWord(op.mem_addr);
            } else {
                return m_memory->readByte(op.mem_addr);
            }

        case Operand::IMMEDIATE:
            return op.imm_value;

        default:
            return 0;
    }
}

void Decoder::writeOperand(const Operand& op, uint16 value, bool is_word) {
    switch (op.type) {
        case Operand::REGISTER_8:
            m_cpu->setReg8(op.reg8, static_cast<uint8>(value));
            break;

        case Operand::REGISTER_16:
            m_cpu->setReg16(op.reg16, value);
            break;

        case Operand::MEMORY:
            if (is_word) {
                m_memory->writeWord(op.mem_addr, value);
            } else {
                m_memory->writeByte(op.mem_addr, static_cast<uint8>(value));
            }
            break;

        default:
            break;
    }
}

} // namespace e2emu
