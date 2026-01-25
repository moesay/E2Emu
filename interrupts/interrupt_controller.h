#pragma once

/**
 * @file interrupt_controller.h
 * @brief BIOS interrupt handling for the 8086 emulator.
 */

#include "../core/types.h"
#include "../core/cpu.h"
#include "../core/memory.h"
#include "../devices/vga_device.h"
#include <functional>
#include <map>

namespace e2emu {

/**
 * @brief Interrupt controller handling BIOS and DOS interrupts.
 *
 * Provides software emulation of common BIOS interrupts (INT 10h, INT 16h)
 * and DOS interrupts (INT 21h). Supports custom interrupt handlers for
 * extensibility.
 */
class InterruptController {
public:
    /**
     * @brief Construct interrupt controller.
     * @param cpu Pointer to CPU state.
     * @param memory Pointer to system memory.
     * @param vga Pointer to VGA device for video services.
     */
    InterruptController(CPU* cpu, Memory* memory, VGADevice* vga);

    /**
     * @brief Handle an interrupt.
     * @param interrupt_num Interrupt number (0-255).
     * @return true to continue execution, false to halt.
     */
    bool handleInterrupt(uint8 interrupt_num);

    /**
     * @brief Set custom handler for an interrupt.
     * @param interrupt_num Interrupt number.
     * @param handler Handler function returning true to continue, false to halt.
     */
    void setHandler(uint8 interrupt_num, std::function<bool()> handler);

    /**
     * @brief Remove all custom handlers.
     */
    void clearHandlers();

    /// @name Keyboard support for UI integration
    /// @{

    /**
     * @brief Inject a keystroke into the keyboard buffer.
     * @param ascii ASCII character code.
     * @param scancode Keyboard scan code.
     */
    void injectKeystroke(uint8 ascii, uint8 scancode);

    /**
     * @brief Inject a keystroke (combined format).
     * @param keystroke Combined value: AH=scancode, AL=ascii.
     */
    void injectKeystroke(uint16 keystroke);

    /**
     * @brief Check if keyboard buffer has pending keystrokes.
     * @return true if keystroke available.
     */
    bool hasKeystroke() const { return !m_keyboard_buffer.empty(); }

    /**
     * @brief Clear keyboard buffer.
     */
    void clearKeyboardBuffer() { m_keyboard_buffer.clear(); }

    /// @}

private:
    CPU* m_cpu;
    Memory* m_memory;
    VGADevice* m_vga;

    std::map<uint8, std::function<bool()>> m_custom_handlers;
    std::vector<uint16> m_keyboard_buffer;

    /// @name BIOS interrupt handlers
    /// @{
    bool handleInt10();  ///< Video services
    bool handleInt16();  ///< Keyboard services
    bool handleInt21();  ///< DOS services
    bool handleInt13();  ///< Disk services (stub)
    /// @}

    /// @name INT 10h - Video services
    /// @{
    void int10_SetVideoMode();
    void int10_SetCursorPosition();
    void int10_GetCursorPosition();
    void int10_TeletypeOutput();
    void int10_WriteChar();
    void int10_ReadChar();
    void int10_SetPixel();
    void int10_GetPixel();
    /// @}

    /// @name INT 16h - Keyboard services
    /// @{
    void int16_ReadKeystroke();
    void int16_CheckKeystroke();
    /// @}

    /// @name INT 21h - DOS services
    /// @{
    void int21_PrintCharacter();
    void int21_PrintString();
    void int21_ReadCharacter();
    void int21_Exit();
    /// @}
};

} // namespace e2emu
