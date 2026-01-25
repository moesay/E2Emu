#pragma once

/**
 * @file vga_device.h
 * @brief VGA display device emulation.
 */

#include "port_controller.h"
#include "../core/memory.h"
#include <vector>
#include <functional>

namespace e2emu {

/**
 * @brief VGA display device emulation.
 *
 * Emulates basic VGA functionality including text mode (80x25)
 * and graphics mode (320x200x256). Handles CRT controller ports
 * and memory-mapped display buffers.
 */
class VGADevice : public IODevice {
public:
    /**
     * @brief Supported video modes.
     */
    enum class VideoMode {
        TEXT_80x25_16COLOR = 0x03,       ///< Standard 80x25 text mode
        GRAPHICS_320x200_256COLOR = 0x13, ///< Mode 13h (320x200, 256 colors)
        UNKNOWN = 0xFF
    };

    /// @name Text mode constants
    /// @{
    static constexpr int TEXT_WIDTH = 80;
    static constexpr int TEXT_HEIGHT = 25;
    static constexpr int TEXT_BUFFER_SIZE = TEXT_WIDTH * TEXT_HEIGHT * 2;
    /// @}

    /// @name Graphics mode constants
    /// @{
    static constexpr int GFX_WIDTH = 320;
    static constexpr int GFX_HEIGHT = 200;
    static constexpr int GFX_BUFFER_SIZE = GFX_WIDTH * GFX_HEIGHT;
    /// @}

    /// @name Memory-mapped buffer addresses
    /// @{
    static constexpr PhysicalAddress TEXT_BUFFER_ADDR = 0xB8000;
    static constexpr PhysicalAddress GFX_BUFFER_ADDR = 0xA0000;
    /// @}

    /**
     * @brief Construct VGA device.
     * @param memory Pointer to system memory for buffer mapping.
     */
    VGADevice(Memory* memory);

    /// @name IODevice interface
    /// @{
    uint8 portIn(uint16 port) override;
    void portOut(uint16 port, uint8 value) override;
    std::string getName() const override { return "VGA"; }
    /// @}

    /**
     * @brief Set video mode.
     * @param mode Video mode to set.
     */
    void setVideoMode(VideoMode mode);

    /**
     * @brief Get current video mode.
     * @return Current video mode.
     */
    VideoMode getVideoMode() const { return m_current_mode; }

    /// @name Text mode operations
    /// @{
    void putChar(int x, int y, char ch, uint8 attribute);
    char getChar(int x, int y) const;
    uint8 getAttribute(int x, int y) const;
    void setCursorPosition(int x, int y);
    void getCursorPosition(int& x, int& y) const;
    /// @}

    /// @name Graphics mode operations
    /// @{
    void putPixel(int x, int y, uint8 color);
    uint8 getPixel(int x, int y) const;
    /// @}

    /**
     * @brief Set screen update callback.
     * @param callback Function called when display changes.
     */
    void setUpdateCallback(std::function<void()> callback) {
        m_update_callback = callback;
    }

    /// @name Buffer access for UI rendering
    /// @{
    const std::vector<uint8>& getTextBuffer() const { return m_text_buffer; }
    const std::vector<uint8>& getGraphicsBuffer() const { return m_graphics_buffer; }
    /// @}

    /**
     * @brief Output character in teletype mode (handles newlines, etc).
     * @param ch Character to output.
     */
    void teletypeOutput(char ch);

    /**
     * @brief Clear the screen.
     */
    void clearScreen();

    /**
     * @brief Sync internal buffer from memory-mapped region.
     */
    void syncBufferFromMemory();

    /**
     * @brief Sync memory-mapped region from internal buffer.
     */
    void syncBufferToMemory();

private:
    Memory* m_memory;
    VideoMode m_current_mode;

    int m_cursor_x;
    int m_cursor_y;
    bool m_cursor_visible;

    std::vector<uint8> m_text_buffer;
    std::vector<uint8> m_graphics_buffer;

    std::function<void()> m_update_callback;

    uint8 m_crtc_index;
    uint8 m_crtc_data;

    void notifyUpdate();
    void scrollUp();
};

} // namespace e2emu
