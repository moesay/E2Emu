#pragma once

/**
 * @file types.h
 * @brief Core type definitions and structures for the 8086 emulator.
 */

#include <cstdint>
#include <string>
#include <functional>

namespace e2emu {

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;

/**
 * @brief Physical memory address type.
 *
 * The 8086 uses 20-bit addressing (segment:offset), allowing access
 * to 1MB of memory.
 */
using PhysicalAddress = uint32;

/**
 * @brief CPU flags register.
 *
 * Represents the 8086 FLAGS register with individual flag bits.
 * Bit layout follows the original 8086 specification.
 */
struct Flags {
    bool CF : 1;  ///< Carry Flag
    bool _1 : 1;  ///< Reserved (always 1)
    bool PF : 1;  ///< Parity Flag
    bool _0a : 1; ///< Reserved (always 0)
    bool AF : 1;  ///< Auxiliary Carry Flag
    bool _0b : 1; ///< Reserved (always 0)
    bool ZF : 1;  ///< Zero Flag
    bool SF : 1;  ///< Sign Flag
    bool TF : 1;  ///< Trap Flag
    bool IF : 1;  ///< Interrupt Enable Flag
    bool DF : 1;  ///< Direction Flag
    bool OF : 1;  ///< Overflow Flag
    uint16 _reserved : 4; ///< Reserved bits

    Flags() : CF(0), _1(1), PF(0), _0a(0), AF(0), _0b(0),
              ZF(0), SF(0), TF(0), IF(0), DF(0), OF(0), _reserved(0) {}

    /**
     * @brief Convert flags to 16-bit word representation.
     * @return Flags as a uint16 value.
     */
    uint16 toWord() const;

    /**
     * @brief Load flags from 16-bit word.
     * @param value The word value to load flags from.
     */
    void fromWord(uint16 value);
};

/**
 * @brief 8-bit register indices.
 */
enum class Reg8 : uint8 {
    AL = 0, CL, DL, BL, AH, CH, DH, BH
};

/**
 * @brief 16-bit general purpose register indices.
 */
enum class Reg16 : uint8 {
    AX = 0, CX, DX, BX, SP, BP, SI, DI
};

/**
 * @brief Segment register indices.
 */
enum class SegReg : uint8 {
    ES = 0, CS, SS, DS
};

/**
 * @brief CPU execution state.
 */
enum class ExecutionState {
    RUNNING,    ///< CPU is executing instructions
    HALTED,     ///< CPU is halted (HLT instruction)
    ERROR,      ///< CPU encountered an error
    BREAKPOINT  ///< CPU hit a breakpoint
};

/**
 * @brief Callback functions for UI integration.
 *
 * These callbacks allow the emulator to notify external code
 * (such as a GUI) about state changes and events.
 */
struct EmulatorCallbacks {
    std::function<void()> onInstructionExecuted;  ///< Called after each instruction
    std::function<void()> onRegistersChanged;     ///< Called when registers change
    std::function<void(PhysicalAddress addr, uint8 value)> onMemoryChanged;  ///< Called on memory writes
    std::function<void()> onScreenUpdated;        ///< Called when VGA buffer changes
    std::function<void(const std::string& error)> onError;  ///< Called on errors
    std::function<void(ExecutionState state)> onStateChanged;  ///< Called on state changes
    std::function<void(uint16 port, uint8 value, bool is_input)> onPortAccess;  ///< Called on IN/OUT
    std::function<uint8(uint16 port)> onPortInputRequest;  ///< Called when IN needs user input
};

} // namespace e2emu
