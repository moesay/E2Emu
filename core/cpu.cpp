/**
 * @file cpu.cpp
 * @brief CPU state and register management implementation.
 */

#include "cpu.h"
#include <cstring>

namespace e2emu {

uint16 Flags::toWord() const {
    uint16 result = 0;
    if (CF) result |= (1 << 0);
    result |= (1 << 1);  // Bit 1 always 1
    if (PF) result |= (1 << 2);
    if (AF) result |= (1 << 4);
    if (ZF) result |= (1 << 6);
    if (SF) result |= (1 << 7);
    if (TF) result |= (1 << 8);
    if (IF) result |= (1 << 9);
    if (DF) result |= (1 << 10);
    if (OF) result |= (1 << 11);
    return result;
}

void Flags::fromWord(uint16 value) {
    CF = (value & (1 << 0)) != 0;
    PF = (value & (1 << 2)) != 0;
    AF = (value & (1 << 4)) != 0;
    ZF = (value & (1 << 6)) != 0;
    SF = (value & (1 << 7)) != 0;
    TF = (value & (1 << 8)) != 0;
    IF = (value & (1 << 9)) != 0;
    DF = (value & (1 << 10)) != 0;
    OF = (value & (1 << 11)) != 0;
}

CPU::CPU() : state(ExecutionState::HALTED) {
    reset();
}

void CPU::reset() {
    // Clear all registers
    std::memset(&regs16, 0, sizeof(regs16));
    std::memset(&seg_regs, 0, sizeof(seg_regs));

    // Set initial values (8086 reset vector)
    CS = 0xFFFF;
    IP = 0x0000;

    // Initialize stack
    SS = 0x0000;
    SP = 0xFFFE;

    // Clear flags
    flags = Flags();

    state = ExecutionState::HALTED;
}

uint8 CPU::getReg8(Reg8 reg) const {
    uint8 index = static_cast<uint8>(reg);
    if (index < 4) {
        // AL, CL, DL, BL (low bytes)
        return lowByte(regs16[index]);
    } else {
        // AH, CH, DH, BH (high bytes)
        return highByte(regs16[index - 4]);
    }
}

void CPU::setReg8(Reg8 reg, uint8 value) {
    uint8 index = static_cast<uint8>(reg);
    if (index < 4) {
        // AL, CL, DL, BL (low bytes)
        lowByte(regs16[index]) = value;
    } else {
        // AH, CH, DH, BH (high bytes)
        highByte(regs16[index - 4]) = value;
    }
}

uint16 CPU::getReg16(Reg16 reg) const {
    return regs16[static_cast<uint8>(reg)];
}

void CPU::setReg16(Reg16 reg, uint16 value) {
    regs16[static_cast<uint8>(reg)] = value;
}

uint16 CPU::getSegReg(SegReg reg) const {
    return seg_regs[static_cast<uint8>(reg)];
}

void CPU::setSegReg(SegReg reg, uint16 value) {
    seg_regs[static_cast<uint8>(reg)] = value;
}

PhysicalAddress CPU::calculatePhysicalAddress(uint16 segment, uint16 offset) const {
    // 8086 addressing: physical = (segment << 4) + offset
    return (static_cast<PhysicalAddress>(segment) << 4) + offset;
}

std::string CPU::getRegName(Reg8 reg) {
    static const char* names[] = {"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"};
    return names[static_cast<uint8>(reg)];
}

std::string CPU::getRegName(Reg16 reg) {
    static const char* names[] = {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"};
    return names[static_cast<uint8>(reg)];
}

std::string CPU::getRegName(SegReg reg) {
    static const char* names[] = {"ES", "CS", "SS", "DS"};
    return names[static_cast<uint8>(reg)];
}

} // namespace e2emu
