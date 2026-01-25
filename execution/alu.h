#pragma once

/**
 * @file alu.h
 * @brief Arithmetic Logic Unit for the 8086 emulator.
 */

#include "../core/types.h"
#include "../core/cpu.h"

namespace e2emu {

/**
 * @brief Arithmetic Logic Unit.
 *
 * Implements all arithmetic, logic, and shift operations for the 8086.
 * Automatically updates CPU flags based on operation results.
 */
class ALU {
public:
    /**
     * @brief Construct ALU attached to a CPU.
     * @param cpu Pointer to CPU for flag updates.
     */
    ALU(CPU* cpu);

    /// @name Arithmetic operations
    /// @{

    /**
     * @brief Add two values.
     * @param dst Destination value.
     * @param src Source value.
     * @param is_word True for 16-bit, false for 8-bit.
     * @param with_carry Add carry flag (ADC instruction).
     * @return Result.
     */
    uint16 add(uint16 dst, uint16 src, bool is_word, bool with_carry = false);

    /**
     * @brief Subtract two values.
     * @param dst Destination value.
     * @param src Source value.
     * @param is_word True for 16-bit, false for 8-bit.
     * @param with_borrow Subtract borrow flag (SBB instruction).
     * @return Result.
     */
    uint16 sub(uint16 dst, uint16 src, bool is_word, bool with_borrow = false);

    /**
     * @brief Multiply. Result stored in AX (and DX for word).
     * @param src Multiplier.
     * @param is_word True for 16-bit, false for 8-bit.
     * @param is_signed True for IMUL, false for MUL.
     * @return 0 (result is in AX/DX).
     */
    uint16 mul(uint16 src, bool is_word, bool is_signed);

    /**
     * @brief Divide.
     * @param dividend_high High word of dividend (DX for word ops).
     * @param dividend_low Low word of dividend (AX).
     * @param divisor Divisor value.
     * @param is_word True for 16-bit, false for 8-bit.
     * @param is_signed True for IDIV, false for DIV.
     * @param remainder Output: remainder value.
     * @return Quotient.
     */
    uint16 div(uint16 dividend_high, uint16 dividend_low, uint16 divisor, bool is_word, bool is_signed, uint16& remainder);

    /**
     * @brief Increment value. Does not affect CF.
     * @param value Value to increment.
     * @param is_word True for 16-bit, false for 8-bit.
     * @return Result.
     */
    uint16 inc(uint16 value, bool is_word);

    /**
     * @brief Decrement value. Does not affect CF.
     * @param value Value to decrement.
     * @param is_word True for 16-bit, false for 8-bit.
     * @return Result.
     */
    uint16 dec(uint16 value, bool is_word);

    /**
     * @brief Negate value (two's complement).
     * @param value Value to negate.
     * @param is_word True for 16-bit, false for 8-bit.
     * @return Result.
     */
    uint16 neg(uint16 value, bool is_word);

    /// @}

    /// @name Logic operations
    /// @{
    uint16 and_op(uint16 dst, uint16 src, bool is_word);
    uint16 or_op(uint16 dst, uint16 src, bool is_word);
    uint16 xor_op(uint16 dst, uint16 src, bool is_word);
    uint16 not_op(uint16 value, bool is_word);
    void test(uint16 dst, uint16 src, bool is_word);
    void cmp(uint16 dst, uint16 src, bool is_word);
    /// @}

    /// @name Shift and rotate operations
    /// @{
    uint16 shl(uint16 value, uint8 count, bool is_word);
    uint16 shr(uint16 value, uint8 count, bool is_word);
    uint16 sar(uint16 value, uint8 count, bool is_word);
    uint16 rol(uint16 value, uint8 count, bool is_word);
    uint16 ror(uint16 value, uint8 count, bool is_word);
    uint16 rcl(uint16 value, uint8 count, bool is_word);
    uint16 rcr(uint16 value, uint8 count, bool is_word);
    /// @}

    /// @name BCD operations
    /// @{
    void aaa();  ///< ASCII adjust after addition
    void aas();  ///< ASCII adjust after subtraction
    void aam(uint8 base = 10);  ///< ASCII adjust after multiplication
    void aad(uint8 base = 10);  ///< ASCII adjust before division
    void daa();  ///< Decimal adjust after addition
    void das();  ///< Decimal adjust after subtraction
    /// @}

private:
    CPU* m_cpu;

    void setFlags_Add(uint32 result, uint16 dst, uint16 src, bool is_word);
    void setFlags_Sub(uint32 result, uint16 dst, uint16 src, bool is_word);
    void setFlags_Logic(uint16 result, bool is_word);
    void setFlags_Shift(uint16 result, bool is_word, bool carry_out);

    void updateParity(uint8 value);
    void updateSign(uint16 value, bool is_word);
    void updateZero(uint16 value, bool is_word);
};

} // namespace e2emu
