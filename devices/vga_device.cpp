/**
 * @file vga_device.cpp
 * @brief VGA display device implementation.
 */

#include "vga_device.h"
#include <cstring>
#include <algorithm>

namespace e2emu {

VGADevice::VGADevice(Memory* memory)
    : m_memory(memory)
    , m_current_mode(VideoMode::TEXT_80x25_16COLOR)
    , m_cursor_x(0)
    , m_cursor_y(0)
    , m_cursor_visible(true)
    , m_crtc_index(0)
    , m_crtc_data(0)
{
    m_text_buffer.resize(TEXT_BUFFER_SIZE, 0);
    m_graphics_buffer.resize(GFX_BUFFER_SIZE, 0);

    clearScreen();
}

uint8 VGADevice::portIn(uint16 port) {
    switch (port) {
        case 0x3D4:  // CRT Controller Index Register
            return m_crtc_index;

        case 0x3D5:  // CRT Controller Data Register
            return m_crtc_data;

        case 0x3DA:  // Input Status Register
            // Bit 0: Display enable (always 1)
            // Bit 3: Vertical retrace (toggle for compatibility)
            static bool vretrace = false;
            vretrace = !vretrace;
            return vretrace ? 0x08 : 0x00;

        default:
            return 0xFF;
    }
}

void VGADevice::portOut(uint16 port, uint8 value) {
    switch (port) {
        case 0x3D4:  // CRT Controller Index Register
            m_crtc_index = value;
            break;

        case 0x3D5:  // CRT Controller Data Register
            m_crtc_data = value;
            // Handle cursor position updates
            if (m_crtc_index == 0x0E) {
                // Cursor Location High
                int offset = m_cursor_y * TEXT_WIDTH + m_cursor_x;
                offset = (offset & 0x00FF) | (value << 8);
                m_cursor_x = offset % TEXT_WIDTH;
                m_cursor_y = offset / TEXT_WIDTH;
            } else if (m_crtc_index == 0x0F) {
                // Cursor Location Low
                int offset = m_cursor_y * TEXT_WIDTH + m_cursor_x;
                offset = (offset & 0xFF00) | value;
                m_cursor_x = offset % TEXT_WIDTH;
                m_cursor_y = offset / TEXT_WIDTH;
            }
            break;

        default:
            break;
    }
}

void VGADevice::setVideoMode(VideoMode mode) {
    m_current_mode = mode;

    if (mode == VideoMode::TEXT_80x25_16COLOR) {
        std::fill(m_text_buffer.begin(), m_text_buffer.end(), 0);
        syncBufferToMemory();
    } else if (mode == VideoMode::GRAPHICS_320x200_256COLOR) {
        std::fill(m_graphics_buffer.begin(), m_graphics_buffer.end(), 0);
    }

    m_cursor_x = 0;
    m_cursor_y = 0;
    notifyUpdate();
}

void VGADevice::putChar(int x, int y, char ch, uint8 attribute) {
    if (x < 0 || x >= TEXT_WIDTH || y < 0 || y >= TEXT_HEIGHT) {
        return;
    }

    int offset = (y * TEXT_WIDTH + x) * 2;
    m_text_buffer[offset] = static_cast<uint8>(ch);
    m_text_buffer[offset + 1] = attribute;

    // Write to memory-mapped buffer
    m_memory->writeByte(TEXT_BUFFER_ADDR + offset, static_cast<uint8>(ch));
    m_memory->writeByte(TEXT_BUFFER_ADDR + offset + 1, attribute);

    notifyUpdate();
}

char VGADevice::getChar(int x, int y) const {
    if (x < 0 || x >= TEXT_WIDTH || y < 0 || y >= TEXT_HEIGHT) {
        return 0;
    }

    int offset = (y * TEXT_WIDTH + x) * 2;
    return static_cast<char>(m_text_buffer[offset]);
}

uint8 VGADevice::getAttribute(int x, int y) const {
    if (x < 0 || x >= TEXT_WIDTH || y < 0 || y >= TEXT_HEIGHT) {
        return 0;
    }

    int offset = (y * TEXT_WIDTH + x) * 2;
    return m_text_buffer[offset + 1];
}

void VGADevice::setCursorPosition(int x, int y) {
    m_cursor_x = std::clamp(x, 0, TEXT_WIDTH - 1);
    m_cursor_y = std::clamp(y, 0, TEXT_HEIGHT - 1);

    // Update CRTC cursor location registers
    int offset = m_cursor_y * TEXT_WIDTH + m_cursor_x;
    m_crtc_data = static_cast<uint8>(offset & 0xFF);  // Low byte
    // High byte would be set via port 0x3D5 with index 0x0E
}

void VGADevice::getCursorPosition(int& x, int& y) const {
    x = m_cursor_x;
    y = m_cursor_y;
}

void VGADevice::putPixel(int x, int y, uint8 color) {
    if (x < 0 || x >= GFX_WIDTH || y < 0 || y >= GFX_HEIGHT) {
        return;
    }

    int offset = y * GFX_WIDTH + x;
    m_graphics_buffer[offset] = color;

    // Write to memory-mapped buffer
    m_memory->writeByte(GFX_BUFFER_ADDR + offset, color);

    notifyUpdate();
}

uint8 VGADevice::getPixel(int x, int y) const {
    if (x < 0 || x >= GFX_WIDTH || y < 0 || y >= GFX_HEIGHT) {
        return 0;
    }

    int offset = y * GFX_WIDTH + x;
    return m_graphics_buffer[offset];
}

void VGADevice::teletypeOutput(char ch) {
    // Handle special characters
    if (ch == '\n') {
        // Newline: move to start of next line
        m_cursor_x = 0;
        m_cursor_y++;
        if (m_cursor_y >= TEXT_HEIGHT) {
            scrollUp();
            m_cursor_y = TEXT_HEIGHT - 1;
        }
    } else if (ch == '\r') {
        // Carriage return: move to start of current line
        m_cursor_x = 0;
    } else if (ch == '\b') {
        // Backspace
        if (m_cursor_x > 0) {
            m_cursor_x--;
            putChar(m_cursor_x, m_cursor_y, ' ', 0x07);
        }
    } else if (ch == '\t') {
        // Tab: advance to next multiple of 8
        m_cursor_x = (m_cursor_x + 8) & ~7;
        if (m_cursor_x >= TEXT_WIDTH) {
            m_cursor_x = 0;
            m_cursor_y++;
            if (m_cursor_y >= TEXT_HEIGHT) {
                scrollUp();
                m_cursor_y = TEXT_HEIGHT - 1;
            }
        }
    } else {
        // Regular character: print at cursor position
        putChar(m_cursor_x, m_cursor_y, ch, 0x07);  // White on black
        m_cursor_x++;

        if (m_cursor_x >= TEXT_WIDTH) {
            m_cursor_x = 0;
            m_cursor_y++;
            if (m_cursor_y >= TEXT_HEIGHT) {
                scrollUp();
                m_cursor_y = TEXT_HEIGHT - 1;
            }
        }
    }
}

void VGADevice::clearScreen() {
    if (m_current_mode == VideoMode::TEXT_80x25_16COLOR) {
        // Clear text buffer with spaces
        for (int y = 0; y < TEXT_HEIGHT; y++) {
            for (int x = 0; x < TEXT_WIDTH; x++) {
                putChar(x, y, ' ', 0x07);  // Space with white on black
            }
        }
    } else if (m_current_mode == VideoMode::GRAPHICS_320x200_256COLOR) {
        // Clear graphics buffer to black
        std::fill(m_graphics_buffer.begin(), m_graphics_buffer.end(), 0);
        for (size_t i = 0; i < m_graphics_buffer.size(); i++) {
            m_memory->writeByte(GFX_BUFFER_ADDR + i, 0);
        }
    }

    m_cursor_x = 0;
    m_cursor_y = 0;
    notifyUpdate();
}

void VGADevice::syncBufferFromMemory() {
    if (m_current_mode == VideoMode::TEXT_80x25_16COLOR) {
        for (size_t i = 0; i < m_text_buffer.size(); i++) {
            m_text_buffer[i] = m_memory->readByte(TEXT_BUFFER_ADDR + i);
        }
    } else if (m_current_mode == VideoMode::GRAPHICS_320x200_256COLOR) {
        for (size_t i = 0; i < m_graphics_buffer.size(); i++) {
            m_graphics_buffer[i] = m_memory->readByte(GFX_BUFFER_ADDR + i);
        }
    }
}

void VGADevice::syncBufferToMemory() {
    if (m_current_mode == VideoMode::TEXT_80x25_16COLOR) {
        for (size_t i = 0; i < m_text_buffer.size(); i++) {
            m_memory->writeByte(TEXT_BUFFER_ADDR + i, m_text_buffer[i]);
        }
    } else if (m_current_mode == VideoMode::GRAPHICS_320x200_256COLOR) {
        for (size_t i = 0; i < m_graphics_buffer.size(); i++) {
            m_memory->writeByte(GFX_BUFFER_ADDR + i, m_graphics_buffer[i]);
        }
    }
}

void VGADevice::notifyUpdate() {
    if (m_update_callback) {
        m_update_callback();
    }
}

void VGADevice::scrollUp() {
    // Move all lines up by one
    for (int y = 0; y < TEXT_HEIGHT - 1; y++) {
        for (int x = 0; x < TEXT_WIDTH; x++) {
            char ch = getChar(x, y + 1);
            uint8 attr = getAttribute(x, y + 1);
            putChar(x, y, ch, attr);
        }
    }

    // Clear the last line
    for (int x = 0; x < TEXT_WIDTH; x++) {
        putChar(x, TEXT_HEIGHT - 1, ' ', 0x07);
    }

    notifyUpdate();
}

} // namespace e2emu
