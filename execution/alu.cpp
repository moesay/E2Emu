/**
 * @file alu.cpp
 * @brief ALU implementation for arithmetic, logic, and shift operations.
 */

#include "alu.h"

namespace e2emu {

ALU::ALU(CPU* cpu) : m_cpu(cpu) {
}

uint16 ALU::add(uint16 dst, uint16 src, bool is_word, bool with_carry) {
    uint32 carry = with_carry && m_cpu->flags.CF ? 1 : 0;
    uint32 result = dst + src + carry;

    setFlags_Add(result, dst, src, is_word);

    return is_word ? (result & 0xFFFF) : (result & 0xFF);
}

uint16 ALU::sub(uint16 dst, uint16 src, bool is_word, bool with_borrow) {
    uint32 borrow = with_borrow && m_cpu->flags.CF ? 1 : 0;
    uint32 result = dst - src - borrow;

    setFlags_Sub(result, dst, src, is_word);

    return is_word ? (result & 0xFFFF) : (result & 0xFF);
}

uint16 ALU::mul(uint16 src, bool is_word, bool is_signed) {
    if (is_word) {
        if (is_signed) {
            // IMUL: signed 16x16 -> 32
            int32_t result = static_cast<int16_t>(m_cpu->AX) * static_cast<int16_t>(src);
            m_cpu->AX = static_cast<uint16>(result & 0xFFFF);
            m_cpu->DX = static_cast<uint16>((result >> 16) & 0xFFFF);
            // CF and OF set if high word (DX) is sign extension of low word (AX)
            m_cpu->flags.CF = m_cpu->flags.OF = (m_cpu->DX != (m_cpu->AX & 0x8000 ? 0xFFFF : 0x0000));
        } else {
            // MUL: unsigned 16x16 -> 32
            uint32_t result = static_cast<uint32_t>(m_cpu->AX) * static_cast<uint32_t>(src);
            m_cpu->AX = static_cast<uint16>(result & 0xFFFF);
            m_cpu->DX = static_cast<uint16>((result >> 16) & 0xFFFF);
            // CF and OF set if high word (DX) is non-zero
            m_cpu->flags.CF = m_cpu->flags.OF = (m_cpu->DX != 0);
        }
    } else {
        uint8_t al = m_cpu->getReg8(Reg8::AL);
        if (is_signed) {
            // IMUL: signed 8x8 -> 16
            int16_t result = static_cast<int8_t>(al) * static_cast<int8_t>(src);
            m_cpu->AX = static_cast<uint16>(result);
            // CF and OF set if high byte (AH) is sign extension of low byte (AL)
            m_cpu->flags.CF = m_cpu->flags.OF = (m_cpu->getReg8(Reg8::AH) != (al & 0x80 ? 0xFF : 0x00));
        } else {
            // MUL: unsigned 8x8 -> 16
            uint16_t result = static_cast<uint16_t>(al) * static_cast<uint16_t>(src);
            m_cpu->AX = result;
            // CF and OF set if high byte (AH) is non-zero
            m_cpu->flags.CF = m_cpu->flags.OF = (m_cpu->getReg8(Reg8::AH) != 0);
        }
    }

    return 0;  // Result is in AX (and DX for word operations)
}

uint16 ALU::div(uint16 dividend_high, uint16 dividend_low, uint16 divisor, bool is_word, bool is_signed, uint16& remainder) {
    if (divisor == 0) {
        // Maybe should fire an interrupt ?
        return 0;
    }

    if (is_word) {
        if (is_signed) {
            // IDIV: signed 32/16 -> 16
            int32_t dividend = (static_cast<int32_t>(static_cast<int16_t>(dividend_high)) << 16) | dividend_low;
            int16_t quotient = dividend / static_cast<int16_t>(divisor);
            int16_t rem = dividend % static_cast<int16_t>(divisor);
            remainder = static_cast<uint16>(rem);
            return static_cast<uint16>(quotient);
        } else {
            // DIV: unsigned 32/16 -> 16
            uint32_t dividend = (static_cast<uint32_t>(dividend_high) << 16) | dividend_low;
            uint16_t quotient = dividend / divisor;
            remainder = dividend % divisor;
            return quotient;
        }
    } else {
        if (is_signed) {
            // IDIV: signed 16/8 -> 8
            int16_t dividend = static_cast<int16_t>((dividend_high << 8) | dividend_low);
            int8_t quotient = dividend / static_cast<int8_t>(divisor);
            int8_t rem = dividend % static_cast<int8_t>(divisor);
            remainder = static_cast<uint8>(rem);
            return static_cast<uint8>(quotient);
        } else {
            // DIV: unsigned 16/8 -> 8
            uint16_t dividend = (dividend_high << 8) | dividend_low;
            uint8_t quotient = dividend / static_cast<uint8>(divisor);
            remainder = dividend % static_cast<uint8>(divisor);
            return quotient;
        }
    }
}

// INC is like ADD dst, 1 but it doesn't affect the CF
// so save it, calculate the ADD flags, then restore the CF
uint16 ALU::inc(uint16 value, bool is_word) {
    uint16 result = value + 1;

    bool old_cf = m_cpu->flags.CF;

    setFlags_Add(result, value, 1, is_word);

    m_cpu->flags.CF = old_cf;

    return is_word ? result : (result & 0xFF);
}

// Same as INC
uint16 ALU::dec(uint16 value, bool is_word) {
    uint16 result = value - 1;

    bool old_cf = m_cpu->flags.CF;

    setFlags_Sub(result, value, 1, is_word);

    m_cpu->flags.CF = old_cf;

    return is_word ? result : (result & 0xFF);
}

uint16 ALU::neg(uint16 value, bool is_word) {
    uint16 result = -value;

    // NEG sets CF if value != 0
    m_cpu->flags.CF = (value != 0);

    setFlags_Sub(result, 0, value, is_word);

    return is_word ? result : (result & 0xFF);
}


uint16 ALU::and_op(uint16 dst, uint16 src, bool is_word) {
    uint16 result = dst & src;
    setFlags_Logic(result, is_word);
    return is_word ? result : (result & 0xFF);
}

uint16 ALU::or_op(uint16 dst, uint16 src, bool is_word) {
    uint16 result = dst | src;
    setFlags_Logic(result, is_word);
    return is_word ? result : (result & 0xFF);
}

uint16 ALU::xor_op(uint16 dst, uint16 src, bool is_word) {
    uint16 result = dst ^ src;
    setFlags_Logic(result, is_word);
    return is_word ? result : (result & 0xFF);
}

// NOT doesn't affect flags
uint16 ALU::not_op(uint16 value, bool is_word) {
    uint16 result = ~value;
    return is_word ? result : (result & 0xFF);
}

void ALU::test(uint16 dst, uint16 src, bool is_word) {
    uint16 result = dst & src;
    setFlags_Logic(result, is_word);
}

void ALU::cmp(uint16 dst, uint16 src, bool is_word) {
    uint32 result = dst - src;
    setFlags_Sub(result, dst, src, is_word);
}

uint16 ALU::shl(uint16 value, uint8 count, bool is_word) {
    if (count == 0) return value;

    count &= 0x1F;  // Mask to 5 bits as per the docs

    uint16 mask = is_word ? 0xFFFF : 0xFF;
    uint16 result = value;

    for (uint8 i = 0; i < count; i++) {
        bool msb = (result & (is_word ? 0x8000 : 0x80)) != 0;
        result = (result << 1) & mask;
        m_cpu->flags.CF = msb;
    }

    setFlags_Logic(result, is_word);

    // OF is defined only for count==1
    if (count == 1) {
        bool msb = (result & (is_word ? 0x8000 : 0x80)) != 0;
        m_cpu->flags.OF = (msb != m_cpu->flags.CF);
    }

    return result;
}

uint16 ALU::shr(uint16 value, uint8 count, bool is_word) {
    if (count == 0) return value;

    count &= 0x1F;

    uint16 result = value;

    for (uint8 i = 0; i < count; i++) {
        m_cpu->flags.CF = (result & 1) != 0;
        result >>= 1;
    }

    setFlags_Logic(result, is_word);

    // OF is defined only for count==1
    if (count == 1) {
        m_cpu->flags.OF = (value & (is_word ? 0x8000 : 0x80)) != 0;
    }

    return result;
}

uint16 ALU::sar(uint16 value, uint8 count, bool is_word) {
    if (count == 0) return value;

    count &= 0x1F;

    uint16 sign_bit = value & (is_word ? 0x8000 : 0x80);
    uint16 result = value;

    for (uint8 i = 0; i < count; i++) {
        m_cpu->flags.CF = (result & 1) != 0;
        result = (result >> 1) | sign_bit;
    }

    setFlags_Logic(result, is_word);

    // OF is always 0 for SAR
    if (count == 1) {
        m_cpu->flags.OF = false;
    }

    return result;
}

uint16 ALU::rol(uint16 value, uint8 count, bool is_word) {
    if (count == 0) return value;

    count &= 0x1F;
    uint8 actual_count = is_word ? (count % 16) : (count % 8);

    uint16 mask = is_word ? 0xFFFF : 0xFF;
    uint16 result = value;

    for (uint8 i = 0; i < actual_count; i++) {
        bool msb = (result & (is_word ? 0x8000 : 0x80)) != 0;
        result = ((result << 1) | (msb ? 1 : 0)) & mask;
        m_cpu->flags.CF = msb;
    }

    if (count == 1) {
        bool msb = (result & (is_word ? 0x8000 : 0x80)) != 0;
        m_cpu->flags.OF = (msb != m_cpu->flags.CF);
    }

    return result;
}

uint16 ALU::ror(uint16 value, uint8 count, bool is_word) {
    if (count == 0) return value;

    count &= 0x1F;
    uint8 actual_count = is_word ? (count % 16) : (count % 8);

    uint16 result = value;

    for (uint8 i = 0; i < actual_count; i++) {
        bool lsb = (result & 1) != 0;
        result = (result >> 1) | (lsb ? (is_word ? 0x8000 : 0x80) : 0);
        m_cpu->flags.CF = lsb;
    }

    if (count == 1) {
        bool msb = (result & (is_word ? 0x8000 : 0x80)) != 0;
        bool next_msb = (result & (is_word ? 0x4000 : 0x40)) != 0;
        m_cpu->flags.OF = (msb != next_msb);
    }

    return result;
}

uint16 ALU::rcl(uint16 value, uint8 count, bool is_word) {
    if (count == 0) return value;

    count &= 0x1F;

    uint16 mask = is_word ? 0xFFFF : 0xFF;
    uint16 result = value;
    bool carry = m_cpu->flags.CF;

    for (uint8 i = 0; i < count; i++) {
        bool msb = (result & (is_word ? 0x8000 : 0x80)) != 0;
        result = ((result << 1) | (carry ? 1 : 0)) & mask;
        carry = msb;
    }

    m_cpu->flags.CF = carry;

    if (count == 1) {
        bool msb = (result & (is_word ? 0x8000 : 0x80)) != 0;
        m_cpu->flags.OF = (msb != m_cpu->flags.CF);
    }

    return result;
}

uint16 ALU::rcr(uint16 value, uint8 count, bool is_word) {
    if (count == 0) return value;

    count &= 0x1F;

    uint16 result = value;
    bool carry = m_cpu->flags.CF;

    for (uint8 i = 0; i < count; i++) {
        bool lsb = (result & 1) != 0;
        result = (result >> 1) | (carry ? (is_word ? 0x8000 : 0x80) : 0);
        carry = lsb;
    }

    m_cpu->flags.CF = carry;

    if (count == 1) {
        bool msb = (result & (is_word ? 0x8000 : 0x80)) != 0;
        bool next_msb = (result & (is_word ? 0x4000 : 0x40)) != 0;
        m_cpu->flags.OF = (msb != next_msb);
    }

    return result;
}

void ALU::aaa() {
    uint8_t al = m_cpu->getReg8(Reg8::AL);
    uint8_t ah = m_cpu->getReg8(Reg8::AH);

    if (((al & 0x0F) > 9) || m_cpu->flags.AF) {
        al = (al + 6) & 0x0F;
        ah = ah + 1;
        m_cpu->flags.AF = true;
        m_cpu->flags.CF = true;
    } else {
        al = al & 0x0F;
        m_cpu->flags.AF = false;
        m_cpu->flags.CF = false;
    }

    m_cpu->setReg8(Reg8::AL, al);
    m_cpu->setReg8(Reg8::AH, ah);
}

void ALU::aas() {
    uint8_t al = m_cpu->getReg8(Reg8::AL);
    uint8_t ah = m_cpu->getReg8(Reg8::AH);

    if (((al & 0x0F) > 9) || m_cpu->flags.AF) {
        al = (al - 6) & 0x0F;
        ah = ah - 1;
        m_cpu->flags.AF = true;
        m_cpu->flags.CF = true;
    } else {
        al = al & 0x0F;
        m_cpu->flags.AF = false;
        m_cpu->flags.CF = false;
    }

    m_cpu->setReg8(Reg8::AL, al);
    m_cpu->setReg8(Reg8::AH, ah);
}

void ALU::aam(uint8 base) {
    uint8_t al = m_cpu->getReg8(Reg8::AL);
    uint8_t ah = al / base;
    al = al % base;

    m_cpu->setReg8(Reg8::AL, al);
    m_cpu->setReg8(Reg8::AH, ah);

    updateZero(al, false);
    updateSign(al, false);
    updateParity(al);
}

void ALU::aad(uint8 base) {
    uint8_t al = m_cpu->getReg8(Reg8::AL);
    uint8_t ah = m_cpu->getReg8(Reg8::AH);

    al = ah * base + al;
    ah = 0;

    m_cpu->setReg8(Reg8::AL, al);
    m_cpu->setReg8(Reg8::AH, ah);

    updateZero(al, false);
    updateSign(al, false);
    updateParity(al);
}

void ALU::daa() {
    uint8_t al = m_cpu->getReg8(Reg8::AL);
    uint8_t old_al = al;
    bool old_cf = m_cpu->flags.CF;

    if (((al & 0x0F) > 9) || m_cpu->flags.AF) {
        al += 6;
        m_cpu->flags.CF = old_cf || (al < 6);
        m_cpu->flags.AF = true;
    } else {
        m_cpu->flags.AF = false;
    }

    if ((old_al > 0x99) || old_cf) {
        al += 0x60;
        m_cpu->flags.CF = true;
    } else {
        m_cpu->flags.CF = false;
    }

    m_cpu->setReg8(Reg8::AL, al);
    updateZero(al, false);
    updateSign(al, false);
    updateParity(al);
}

void ALU::das() {
    uint8_t al = m_cpu->getReg8(Reg8::AL);
    uint8_t old_al = al;
    bool old_cf = m_cpu->flags.CF;

    if (((al & 0x0F) > 9) || m_cpu->flags.AF) {
        al -= 6;
        m_cpu->flags.CF = old_cf || (al > old_al);
        m_cpu->flags.AF = true;
    } else {
        m_cpu->flags.AF = false;
    }

    if ((old_al > 0x99) || old_cf) {
        al -= 0x60;
        m_cpu->flags.CF = true;
    }

    m_cpu->setReg8(Reg8::AL, al);
    updateZero(al, false);
    updateSign(al, false);
    updateParity(al);
}

void ALU::setFlags_Add(uint32 result, uint16 dst, uint16 src, bool is_word) {
    uint16 mask = is_word ? 0xFFFF : 0xFF;
    uint16 result_masked = result & mask;

    // Carry flag
    m_cpu->flags.CF = (result > mask);

    // Zero, Sign, Parity
    updateZero(result_masked, is_word);
    updateSign(result_masked, is_word);
    updateParity(static_cast<uint8>(result_masked));

    // Auxiliary carry (bit 3 -> bit 4)
    m_cpu->flags.AF = ((dst & 0xF) + (src & 0xF)) > 0xF;

    // Overflow flag
    uint16 sign_mask = is_word ? 0x8000 : 0x80;
    bool dst_sign = (dst & sign_mask) != 0;
    bool src_sign = (src & sign_mask) != 0;
    bool res_sign = (result_masked & sign_mask) != 0;
    m_cpu->flags.OF = (dst_sign == src_sign) && (dst_sign != res_sign);
}

void ALU::setFlags_Sub(uint32 result, uint16 dst, uint16 src, bool is_word) {
    uint16 mask = is_word ? 0xFFFF : 0xFF;
    uint16 result_masked = result & mask;

    // Carry flag (borrow)
    m_cpu->flags.CF = (src > dst);

    // Zero, Sign, Parity
    updateZero(result_masked, is_word);
    updateSign(result_masked, is_word);
    updateParity(static_cast<uint8>(result_masked));

    // Auxiliary carry (borrow from bit 4)
    m_cpu->flags.AF = (src & 0xF) > (dst & 0xF);

    // Overflow flag
    uint16 sign_mask = is_word ? 0x8000 : 0x80;
    bool dst_sign = (dst & sign_mask) != 0;
    bool src_sign = (src & sign_mask) != 0;
    bool res_sign = (result_masked & sign_mask) != 0;
    m_cpu->flags.OF = (dst_sign != src_sign) && (dst_sign != res_sign);
}

void ALU::setFlags_Logic(uint16 result, bool is_word) {
    // Logic operations clear CF and OF
    m_cpu->flags.CF = false;
    m_cpu->flags.OF = false;

    updateZero(result, is_word);
    updateSign(result, is_word);
    updateParity(static_cast<uint8>(result));

    // AF is undefined for logic operations (leave it unchanged, maybe for now)
}

void ALU::setFlags_Shift(uint16 result, bool is_word, bool carry_out) {
    m_cpu->flags.CF = carry_out;

    updateZero(result, is_word);
    updateSign(result, is_word);
    updateParity(static_cast<uint8>(result));
}

void ALU::updateParity(uint8 value) {
    // Count number of 1 bits in lower 8 bits
    int count = 0;
    for (int i = 0; i < 8; i++) {
        if (value & (1 << i)) count++;
    }
    m_cpu->flags.PF = (count % 2) == 0;  // Even parity
}

void ALU::updateSign(uint16 value, bool is_word) {
    if (is_word) {
        m_cpu->flags.SF = (value & 0x8000) != 0;
    } else {
        m_cpu->flags.SF = (value & 0x80) != 0;
    }
}

void ALU::updateZero(uint16 value, bool is_word) {
    if (is_word) {
        m_cpu->flags.ZF = (value == 0);
    } else {
        m_cpu->flags.ZF = ((value & 0xFF) == 0);
    }
}

} // namespace e2emu
