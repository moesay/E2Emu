/**
 * @file interrupt_controller.cpp
 * @brief BIOS and DOS interrupt handler implementation.
 */

#include "interrupt_controller.h"

namespace e2emu {

InterruptController::InterruptController(CPU* cpu, Memory* memory, VGADevice* vga)
    : m_cpu(cpu)
    , m_memory(memory)
    , m_vga(vga)
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

        case 0x02:  // Set cursor position
            int10_SetCursorPosition();
            break;

        case 0x03:  // Get cursor position
            int10_GetCursorPosition();
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

} // namespace e2emu
