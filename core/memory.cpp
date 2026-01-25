/**
 * @file memory.cpp
 * @brief Memory subsystem implementation.
 */

#include "memory.h"
#include <cstring>
#include <stdexcept>

namespace e2emu {

Memory::Memory() : m_memory(MEMORY_SIZE, 0) {
}

/*
   As per the docs, the HW Itself doesn't have any MMU
   So no exceptions/errors are thrown for any illegal memory read/write
*/

uint8 Memory::readByte(PhysicalAddress addr) const {
    if (!isValidAddress(addr)) {
        return 0xFF;  // Return 0xFF for invalid addresses
    }
    return m_memory[addr];
}

uint16 Memory::readWord(PhysicalAddress addr) const {
    if (!isValidAddress(addr) || !isValidAddress(addr + 1)) {
        return 0xFFFF;
    }

    // Little-endian: low byte first
    uint8 low = m_memory[addr];
    uint8 high = m_memory[addr + 1];
    return (static_cast<uint16>(high) << 8) | low;
}

void Memory::writeByte(PhysicalAddress addr, uint8 value) {
    if (!isValidAddress(addr)) {
        return;  // Ignore writes to invalid addresses
    }

    m_memory[addr] = value;

    if (m_write_callback) {
        m_write_callback(addr, value);
    }
}

void Memory::writeWord(PhysicalAddress addr, uint16 value) {
    if (!isValidAddress(addr) || !isValidAddress(addr + 1)) {
        return;
    }

    // Little-endian: low byte first
    writeByte(addr, static_cast<uint8>(value & 0xFF));
    writeByte(addr + 1, static_cast<uint8>((value >> 8) & 0xFF));
}

void Memory::loadBinary(const std::vector<uint8>& binary, PhysicalAddress start_addr) {
    if (start_addr >= MEMORY_SIZE) {
        return;
    }

    size_t copy_size = std::min(binary.size(), MEMORY_SIZE - start_addr);
    std::memcpy(&m_memory[start_addr], binary.data(), copy_size);
}

std::vector<uint8> Memory::readRange(PhysicalAddress start, size_t length) const {
    if (start >= MEMORY_SIZE) {
        return {};
    }

    size_t actual_length = std::min(length, MEMORY_SIZE - start);
    return std::vector<uint8>(m_memory.begin() + start,
                              m_memory.begin() + start + actual_length);
}

void Memory::clear() {
    std::fill(m_memory.begin(), m_memory.end(), 0);
}

} // namespace e2emu
