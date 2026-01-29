/**
 * @file interrupt_controller.cpp
 * @brief BIOS and DOS interrupt handler implementation.
 */

#include "interrupt_controller.h"
#include <ctime>

namespace e2emu {

InterruptController::InterruptController(CPU* cpu, Memory* memory, VGADevice* vga)
    : m_cpu(cpu)
    , m_memory(memory)
    , m_vga(vga)
    , m_tick_count(0)
    , m_midnight_flag(false)
    , m_start_time(std::chrono::steady_clock::now())
{
}

bool InterruptController::handleInterrupt(uint8 interrupt_num) {
    // Check for custom handlers first
    auto it = m_custom_handlers.find(interrupt_num);
    if (it != m_custom_handlers.end()) {
        return it->second();
    }

    // Handle standard BIOS interrupts
    switch (interrupt_num) {
        case 0x10:
            return handleInt10();  // Video services

        case 0x16:
            return handleInt16();  // Keyboard services

        case 0x21:
            return handleInt21();  // DOS services

        case 0x13:
            return handleInt13();  // Disk services

        case 0x1A:
            return handleInt1A();  // Timer/RTC services

        default:
            // Unhandled interrupt - just return (maybe later will do something)
            return true;
    }
}

void InterruptController::setHandler(uint8 interrupt_num, std::function<bool()> handler) {
    m_custom_handlers[interrupt_num] = handler;
}

void InterruptController::clearHandlers() {
    m_custom_handlers.clear();
}

void InterruptController::injectKeystroke(uint8 ascii, uint8 scancode) {
    // Format: AH=scancode, AL=ascii
    uint16 keystroke = (static_cast<uint16>(scancode) << 8) | ascii;
    m_keyboard_buffer.push_back(keystroke);
}

void InterruptController::injectKeystroke(uint16 keystroke) {
    m_keyboard_buffer.push_back(keystroke);
}

bool InterruptController::handleInt10() {
    uint8 ah = m_cpu->getReg8(Reg8::AH);

    switch (ah) {
        case 0x00:  // Set video mode
            int10_SetVideoMode();
            break;

        case 0x01:  // Set cursor type
            // CH bits 4-0 = cursor start scanline
            // CL bits 4-0 = cursor end scanline
            // CH bit 5 = cursor visibility (1=hidden)
            // The functionality is not emulated
            break;

        case 0x02:  // Set cursor position
            int10_SetCursorPosition();
            break;

        case 0x03:  // Get cursor position
            int10_GetCursorPosition();
            break;

        case 0x05:  // Set active display page
            // AL = page number
            m_memory->writeByte(0x462, m_cpu->getReg8(Reg8::AL));
            break;

        case 0x06:  // Scroll window up
            int10_ScrollUp();
            break;

        case 0x0E:  // Teletype output
            int10_TeletypeOutput();
            break;

        case 0x09:  // Write character and attribute
            int10_WriteChar();
            break;

        case 0x08:  // Read character and attribute at cursor
            int10_ReadChar();
            break;

        case 0x0C:  // Write pixel
            int10_SetPixel();
            break;

        case 0x0D:  // Read pixel
            int10_GetPixel();
            break;

        default:
            // Unimplemented function
            break;
    }

    return true;
}

void InterruptController::int10_SetVideoMode() {
    uint8 mode = m_cpu->getReg8(Reg8::AL);

    if (mode == 0x03) {
        m_vga->setVideoMode(VGADevice::VideoMode::TEXT_80x25_16COLOR);
    } else if (mode == 0x13) {
        m_vga->setVideoMode(VGADevice::VideoMode::GRAPHICS_320x200_256COLOR);
    }
}

void InterruptController::int10_SetCursorPosition() {
    uint8 page = m_cpu->getReg8(Reg8::BH);  // Page number (ignored for now)
    uint8 row = m_cpu->getReg8(Reg8::DH);
    uint8 col = m_cpu->getReg8(Reg8::DL);

    m_vga->setCursorPosition(col, row);
}

void InterruptController::int10_GetCursorPosition() {
    uint8 page = m_cpu->getReg8(Reg8::BH);  // Page number (ignored)

    int x, y;
    m_vga->getCursorPosition(x, y);

    m_cpu->setReg8(Reg8::DH, static_cast<uint8>(y));  // Row
    m_cpu->setReg8(Reg8::DL, static_cast<uint8>(x));  // Column
    m_cpu->setReg8(Reg8::CH, 0);  // Cursor start scanline
    m_cpu->setReg8(Reg8::CL, 7);  // Cursor end scanline
}

void InterruptController::int10_ScrollUp() {
    uint8 lines = m_cpu->getReg8(Reg8::AL);  // Lines to scroll (0 = clear)
    uint8 attr = m_cpu->getReg8(Reg8::BH);   // Fill attribute
    uint8 top_row = m_cpu->getReg8(Reg8::CH);
    uint8 left_col = m_cpu->getReg8(Reg8::CL);
    uint8 bottom_row = m_cpu->getReg8(Reg8::DH);
    uint8 right_col = m_cpu->getReg8(Reg8::DL);

    if (bottom_row >= VGADevice::TEXT_HEIGHT) bottom_row = VGADevice::TEXT_HEIGHT - 1;
    if (right_col >= VGADevice::TEXT_WIDTH) right_col = VGADevice::TEXT_WIDTH - 1;

    if (lines == 0) {
        // Clear the window
        for (int y = top_row; y <= bottom_row; ++y) {
            for (int x = left_col; x <= right_col; ++x) {
                m_vga->putChar(x, y, ' ', attr);
            }
        }
    } else {
        // Scroll up by 'lines' rows
        for (int y = top_row; y <= bottom_row; ++y) {
            for (int x = left_col; x <= right_col; ++x) {
                if (y + lines <= bottom_row) {
                    char ch = m_vga->getChar(x, y + lines);
                    uint8 a = m_vga->getAttribute(x, y + lines);
                    m_vga->putChar(x, y, ch, a);
                } else {
                    m_vga->putChar(x, y, ' ', attr);
                }
            }
        }
    }
}

void InterruptController::int10_TeletypeOutput() {
    char ch = static_cast<char>(m_cpu->getReg8(Reg8::AL));
    m_vga->teletypeOutput(ch);
}

void InterruptController::int10_WriteChar() {
    char ch = static_cast<char>(m_cpu->getReg8(Reg8::AL));
    uint8 attr = m_cpu->getReg8(Reg8::BL);
    uint16 count = m_cpu->getReg16(Reg16::CX);

    int x, y;
    m_vga->getCursorPosition(x, y);

    for (uint16 i = 0; i < count; i++) {
        m_vga->putChar(x + i, y, ch, attr);
    }
}

void InterruptController::int10_ReadChar() {
    int x, y;
    m_vga->getCursorPosition(x, y);

    char ch = m_vga->getChar(x, y);
    uint8 attr = m_vga->getAttribute(x, y);

    m_cpu->setReg8(Reg8::AL, static_cast<uint8>(ch));
    m_cpu->setReg8(Reg8::AH, attr);
}

void InterruptController::int10_SetPixel() {
    uint8 page = m_cpu->getReg8(Reg8::BH);  // Page (ignored)
    uint8 color = m_cpu->getReg8(Reg8::AL);
    uint16 x = m_cpu->getReg16(Reg16::CX);
    uint16 y = m_cpu->getReg16(Reg16::DX);

    m_vga->putPixel(x, y, color);
}

void InterruptController::int10_GetPixel() {
    uint8 page = m_cpu->getReg8(Reg8::BH);  // Page (ignored)
    uint16 x = m_cpu->getReg16(Reg16::CX);
    uint16 y = m_cpu->getReg16(Reg16::DX);

    uint8 color = m_vga->getPixel(x, y);
    m_cpu->setReg8(Reg8::AL, color);
}

bool InterruptController::handleInt16() {
    uint8 ah = m_cpu->getReg8(Reg8::AH);

    switch (ah) {
        case 0x00:  // Read keystroke
            int16_ReadKeystroke();
            break;

        case 0x01:  // Check for keystroke
            int16_CheckKeystroke();
            break;

        default:
            break;
    }

    return true;
}

void InterruptController::int16_ReadKeystroke() {
    if (!m_keyboard_buffer.empty()) {
        uint16 keystroke = m_keyboard_buffer.front();
        m_keyboard_buffer.erase(m_keyboard_buffer.begin());

        m_cpu->setReg16(Reg16::AX, keystroke);
    } else {
        // No key available - BLOCK by re-executing the INT instruction
        // Move IP back by 2 bytes (INT instruction is 2 bytes: CD 16)
        m_cpu->IP -= 2;
    }
}

void InterruptController::int16_CheckKeystroke() {
    if (!m_keyboard_buffer.empty()) {
        // Key available
        uint16 keystroke = m_keyboard_buffer.front();
        m_cpu->setReg16(Reg16::AX, keystroke);
        m_cpu->flags.ZF = false;  // ZF=0 means key available
    } else {
        // No key
        m_cpu->flags.ZF = true;  // ZF=1 means no key
    }
}

bool InterruptController::handleInt21() {
    uint8 ah = m_cpu->getReg8(Reg8::AH);

    switch (ah) {
        case 0x02:  // Character output
            int21_PrintCharacter();
            break;

        case 0x09:  // String output
            int21_PrintString();
            break;

        case 0x01:  // Character input with echo
            int21_ReadCharacter();
            break;

        case 0x4C:  // Exit program
            int21_Exit();
            return false;  // Signal to stop execution

        default:
            break;
    }

    return true;
}

void InterruptController::int21_PrintCharacter() {
    char ch = static_cast<char>(m_cpu->getReg8(Reg8::DL));
    m_vga->teletypeOutput(ch);
}

void InterruptController::int21_PrintString() {
    // DS:DX points to string terminated by '$'
    uint16 segment = m_cpu->DS;
    uint16 offset = m_cpu->getReg16(Reg16::DX);

    PhysicalAddress addr = m_cpu->calculatePhysicalAddress(segment, offset);

    while (true) {
        uint8 ch = m_memory->readByte(addr++);
        if (ch == '$') break;

        m_vga->teletypeOutput(static_cast<char>(ch));
    }
}

void InterruptController::int21_ReadCharacter() {
    if (!m_keyboard_buffer.empty()) {
        uint16 keystroke = m_keyboard_buffer.front();
        m_keyboard_buffer.erase(m_keyboard_buffer.begin());

        uint8 ascii = static_cast<uint8>(keystroke & 0xFF);
        m_cpu->setReg8(Reg8::AL, ascii);

        // Echo character
        m_vga->teletypeOutput(static_cast<char>(ascii));
    } else {
        m_cpu->setReg8(Reg8::AL, 0);
    }
}

void InterruptController::int21_Exit() {
    // Set CPU state to halted
    m_cpu->state = ExecutionState::HALTED;
}

// Disk services not implemented yet, return success until later
bool InterruptController::handleInt13() {
    m_cpu->flags.CF = false;
    m_cpu->setReg8(Reg8::AH, 0);
    return true;
}

bool InterruptController::handleInt1A() {
    uint8 ah = m_cpu->getReg8(Reg8::AH);

    switch (ah) {
        case 0x00:  // Get system time
            int1A_GetSystemTime();
            break;

        case 0x01:  // Set system time
            int1A_SetSystemTime();
            break;

        case 0x02:  // Get RTC time
            int1A_GetRTCTime();
            break;

        case 0x04:  // Get RTC date
            int1A_GetRTCDate();
            break;

        default:
            m_cpu->flags.CF = true;  // Unsupported function
            break;
    }

    return true;
}

void InterruptController::int1A_GetSystemTime() {
    // Calculate ticks since midnight at ~18.2065 Hz
    // Use real wall-clock time for the tick count
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

    uint32_t seconds_since_midnight =
        local->tm_hour * 3600 + local->tm_min * 60 + local->tm_sec;

    // 18.2065 ticks per second (BIOS timer rate: 1193182 / 65536)
    uint32_t ticks = static_cast<uint32_t>(seconds_since_midnight * 18.2065);

    m_cpu->setReg16(Reg16::CX, static_cast<uint16>(ticks >> 16));    // High word
    m_cpu->setReg16(Reg16::DX, static_cast<uint16>(ticks & 0xFFFF)); // Low word
    m_cpu->setReg8(Reg8::AL, m_midnight_flag ? 1 : 0);
    m_midnight_flag = false;
}

void InterruptController::int1A_SetSystemTime() {
    uint16 cx = m_cpu->getReg16(Reg16::CX);
    uint16 dx = m_cpu->getReg16(Reg16::DX);
    m_tick_count = (static_cast<uint32_t>(cx) << 16) | dx;
    m_midnight_flag = false;
}

void InterruptController::int1A_GetRTCTime() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

    // Convert to BCD
    auto toBCD = [](int val) -> uint8 {
        return static_cast<uint8>(((val / 10) << 4) | (val % 10));
    };

    m_cpu->setReg8(Reg8::CH, toBCD(local->tm_hour));   // Hours in BCD
    m_cpu->setReg8(Reg8::CL, toBCD(local->tm_min));    // Minutes in BCD
    m_cpu->setReg8(Reg8::DH, toBCD(local->tm_sec));    // Seconds in BCD
    m_cpu->setReg8(Reg8::DL, 0);                        // Daylight saving (0=standard)
    m_cpu->flags.CF = false;                             // Success
}

void InterruptController::int1A_GetRTCDate() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

    auto toBCD = [](int val) -> uint8 {
        return static_cast<uint8>(((val / 10) << 4) | (val % 10));
    };

    int year = local->tm_year + 1900;
    m_cpu->setReg8(Reg8::CH, toBCD(year / 100));        // Century in BCD
    m_cpu->setReg8(Reg8::CL, toBCD(year % 100));        // Year in BCD
    m_cpu->setReg8(Reg8::DH, toBCD(local->tm_mon + 1)); // Month in BCD
    m_cpu->setReg8(Reg8::DL, toBCD(local->tm_mday));    // Day in BCD
    m_cpu->flags.CF = false;                              // Success
}

} // namespace e2emu
