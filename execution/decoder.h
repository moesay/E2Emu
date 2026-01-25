#pragma once

/**
 * @file decoder.h
 * @brief Instruction decoder for the 8086 emulator.
 */

#include "../core/types.h"
#include "../core/cpu.h"
#include "../core/memory.h"

namespace e2emu {

/**
 * @brief ModR/M byte decoder.
 *
 * The ModR/M byte format is: [MOD(2) REG(3) R/M(3)]
 */
struct ModRM {
    uint8 mod;  ///< Addressing mode (0-3)
    uint8 reg;  ///< Register or opcode extension (0-7)
    uint8 rm;   ///< Register or memory operand (0-7)

    /**
     * @brief Decode a ModR/M byte.
     * @param byte Raw ModR/M byte value.
     */
    ModRM(uint8 byte) {
        mod = (byte >> 6) & 0x03;
        reg = (byte >> 3) & 0x07;
        rm = byte & 0x07;
    }
};

/**
 * @brief Represents an instruction operand.
 */
struct Operand {
    /**
     * @brief Operand type.
     */
    enum Type {
        REGISTER_8,   ///< 8-bit register
        REGISTER_16,  ///< 16-bit register
        MEMORY,       ///< Memory location
        IMMEDIATE     ///< Immediate value
    };

    Type type;
    union {
        Reg8 reg8;
        Reg16 reg16;
        PhysicalAddress mem_addr;
        uint16 imm_value;
    };

    int8_t displacement;
    bool has_displacement;

    Operand() : type(IMMEDIATE), imm_value(0), displacement(0), has_displacement(false) {}
};

/**
 * @brief Instruction decoder.
 *
 * Handles ModR/M byte decoding, effective address calculation,
 * and operand reading/writing.
 */
class Decoder {
public:
    /**
     * @brief Construct decoder.
     * @param cpu Pointer to CPU state.
     * @param memory Pointer to memory.
     */
    Decoder(CPU* cpu, Memory* memory);

    /**
     * @brief Decode ModR/M byte and return operands.
     *
     * Advances IP past the ModR/M byte and any displacement.
     *
     * @param modrm Parsed ModR/M byte.
     * @param is_word True for 16-bit operands.
     * @param dst Output: destination operand (R/M field).
     * @param src Output: source operand (REG field).
     * @param default_seg Default segment for memory access.
     */
    void decodeModRM(ModRM modrm, bool is_word, Operand& dst, Operand& src, SegReg default_seg = SegReg::DS);

    /**
     * @brief Calculate effective address from ModR/M.
     * @param modrm Parsed ModR/M byte.
     * @param seg_override Segment override.
     * @return Physical address.
     */
    PhysicalAddress calculateEffectiveAddress(ModRM modrm, SegReg seg_override = SegReg::DS);

    /**
     * @brief Read value from operand.
     * @param op Operand to read.
     * @param is_word True for 16-bit read.
     * @return Value read.
     */
    uint16 readOperand(const Operand& op, bool is_word);

    /**
     * @brief Write value to operand.
     * @param op Operand to write.
     * @param value Value to write.
     * @param is_word True for 16-bit write.
     */
    void writeOperand(const Operand& op, uint16 value, bool is_word);

    /**
     * @brief Fetch byte at CS:IP and advance IP.
     * @return Byte value.
     */
    uint8 fetchByte();

    /**
     * @brief Fetch word at CS:IP and advance IP by 2.
     * @return Word value.
     */
    uint16 fetchWord();

    /**
     * @brief Set segment override for next memory access.
     * @param seg Segment to use.
     */
    void setSegmentOverride(SegReg seg) { m_seg_override = seg; m_has_seg_override = true; }

    /**
     * @brief Clear segment override.
     */
    void clearSegmentOverride() { m_has_seg_override = false; }

private:
    CPU* m_cpu;
    Memory* m_memory;

    SegReg m_seg_override;
    bool m_has_seg_override;

    int getDisplacementSize(uint8 mod, uint8 rm);
    uint16 calculateBaseAddress(uint8 rm);
};

} // namespace e2emu
