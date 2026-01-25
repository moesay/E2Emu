#pragma once

/**
 * @file cpu.h
 * @brief 8086 CPU state and register management.
 */

#include "types.h"
#include <array>

namespace e2emu {

/**
 * @brief 8086 CPU state.
 *
 * Contains all registers, flags, and execution state for the
 * emulated 8086 processor. Provides methods for register access
 * and physical address calculation.
 */
class CPU {
public:
    CPU();

    /// @name General Purpose Registers (16-bit)
    /// @{
    union {
        struct {
            uint16 AX, CX, DX, BX, SP, BP, SI, DI;
        };
        std::array<uint16, 8> regs16;
    };
    /// @}

    /// @name Segment Registers
    /// @{
    union {
        struct {
            uint16 ES, CS, SS, DS;
        };
        std::array<uint16, 4> seg_regs;
    };
    /// @}

    uint16 IP;        ///< Instruction Pointer
    Flags flags;      ///< FLAGS register
    ExecutionState state;  ///< Current execution state

    /**
     * @brief Get 8-bit register value.
     * @param reg Register to read.
     * @return Register value.
     */
    uint8 getReg8(Reg8 reg) const;

    /**
     * @brief Set 8-bit register value.
     * @param reg Register to write.
     * @param value Value to set.
     */
    void setReg8(Reg8 reg, uint8 value);

    /**
     * @brief Get 16-bit register value.
     * @param reg Register to read.
     * @return Register value.
     */
    uint16 getReg16(Reg16 reg) const;

    /**
     * @brief Set 16-bit register value.
     * @param reg Register to write.
     * @param value Value to set.
     */
    void setReg16(Reg16 reg, uint16 value);

    /**
     * @brief Get segment register value.
     * @param reg Segment register to read.
     * @return Segment value.
     */
    uint16 getSegReg(SegReg reg) const;

    /**
     * @brief Set segment register value.
     * @param reg Segment register to write.
     * @param value Segment value.
     */
    void setSegReg(SegReg reg, uint16 value);

    /**
     * @brief Get FLAGS as 16-bit word.
     * @return FLAGS register value.
     */
    uint16 getFlagsWord() const { return flags.toWord(); }

    /**
     * @brief Set FLAGS from 16-bit word.
     * @param value FLAGS value.
     */
    void setFlagsWord(uint16 value) { flags.fromWord(value); }

    /**
     * @brief Calculate 20-bit physical address from segment:offset.
     * @param segment Segment value.
     * @param offset Offset value.
     * @return Physical address (segment * 16 + offset).
     */
    PhysicalAddress calculatePhysicalAddress(uint16 segment, uint16 offset) const;

    /**
     * @brief Reset CPU to initial power-on state.
     */
    void reset();

    /// @name Register name utilities
    /// @{
    static std::string getRegName(Reg8 reg);
    static std::string getRegName(Reg16 reg);
    static std::string getRegName(SegReg reg);
    /// @}

private:
    uint8& lowByte(uint16& reg) { return reinterpret_cast<uint8*>(&reg)[0]; }
    uint8& highByte(uint16& reg) { return reinterpret_cast<uint8*>(&reg)[1]; }
    const uint8& lowByte(const uint16& reg) const { return reinterpret_cast<const uint8*>(&reg)[0]; }
    const uint8& highByte(const uint16& reg) const { return reinterpret_cast<const uint8*>(&reg)[1]; }
};

} // namespace e2emu
