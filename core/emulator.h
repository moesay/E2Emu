#pragma once

/**
 * @file emulator.h
 * @brief Main emulator class coordinating all components.
 */

#include "types.h"
#include "cpu.h"
#include "memory.h"
#include "../devices/port_controller.h"
#include "../devices/vga_device.h"
#include "../interrupts/interrupt_controller.h"
#include <memory>
#include <string>
#include <vector>

namespace e2emu {

/**
 * @brief Main 8086 emulator class.
 *
 * Coordinates all emulator components including CPU, memory,
 * I/O ports, VGA display, and interrupt handling. Provides
 * the main execution loop and program loading functionality.
 */
class Emulator {
public:
    Emulator();
    ~Emulator();

    /**
     * @brief Load raw binary data into memory.
     * @param binary Binary data to load.
     * @param start_addr Physical address to load at.
     */
    void loadBinary(const std::vector<uint8>& binary, PhysicalAddress start_addr = 0);

    /**
     * @brief Load a program and set up execution environment.
     *
     * Sets up segment registers for COM-style program execution
     * where CS=DS=ES=SS all point to the same segment.
     *
     * @param binary Program binary data.
     * @param code_segment CS value (default 0x0000).
     * @param instruction_pointer Starting IP (default 0x0100 for COM files).
     */
    void loadProgram(const std::vector<uint8>& binary,
                     uint16 code_segment = 0x0000,
                     uint16 instruction_pointer = 0x0100);

    /**
     * @brief Reset emulator to initial state.
     */
    void reset();

    /**
     * @brief Execute a single instruction.
     * @return true if execution can continue, false if halted/error.
     */
    bool step();

    /**
     * @brief Run until HLT or instruction limit reached.
     * @param max_instructions Maximum instructions to execute (0 = unlimited).
     * @return Number of instructions executed.
     */
    size_t run(size_t max_instructions = 0);

    /**
     * @brief Halt the CPU.
     */
    void halt();

    /// @name Component accessors
    /// @{
    const CPU& getCPU() const { return m_cpu; }
    CPU& getCPU() { return m_cpu; }

    const Memory& getMemory() const { return m_memory; }
    Memory& getMemory() { return m_memory; }

    const VGADevice& getVGA() const { return *m_vga; }
    VGADevice& getVGA() { return *m_vga; }

    const PortController& getPortController() const { return m_port_controller; }
    PortController& getPortController() { return m_port_controller; }
    /// @}

    /**
     * @brief Get current execution state.
     * @return Current state (RUNNING, HALTED, ERROR, BREAKPOINT).
     */
    ExecutionState getState() const { return m_cpu.state; }

    /**
     * @brief Get total instruction count since last reset.
     * @return Number of instructions executed.
     */
    size_t getInstructionCount() const { return m_instruction_count; }

    /**
     * @brief Set UI integration callbacks.
     * @param callbacks Callback structure.
     */
    void setCallbacks(const EmulatorCallbacks& callbacks) {
        m_callbacks = callbacks;
    }

    /**
     * @brief Inject a keypress into the keyboard buffer.
     * @param scancode Keyboard scan code.
     * @param ascii ASCII character code.
     */
    void queueKeypress(uint8 scancode, uint8 ascii);

    /**
     * @brief Get disassembly of current instruction.
     * @return String representation of CS:IP and opcode.
     */
    std::string getCurrentInstruction() const;

    /**
     * @brief Get last error message.
     * @return Error string, empty if no error.
     */
    std::string getError() const { return m_error; }

private:
    CPU m_cpu;
    Memory m_memory;
    PortController m_port_controller;

    std::shared_ptr<VGADevice> m_vga;
    std::unique_ptr<InterruptController> m_interrupt_controller;

    size_t m_instruction_count;
    std::string m_error;
    EmulatorCallbacks m_callbacks;

    bool executeInstruction();
    void setError(const std::string& error);
    void notifyInstructionExecuted();
    void notifyRegistersChanged();
    void notifyStateChanged(ExecutionState new_state);
};

} // namespace e2emu
