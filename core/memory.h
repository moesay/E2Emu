#pragma once

/**
 * @file memory.h
 * @brief Memory subsystem for the 8086 emulator.
 */

#include "types.h"
#include <vector>
#include <functional>

namespace e2emu {

/**
 * @brief Memory management for the 8086 emulator.
 *
 * Provides a 1MB address space with 20-bit addressing.
 * Supports callbacks for memory writes to enable UI updates.
 *
 * The 8086 has no MMU, so invalid memory accesses return default
 * values rather than throwing exceptions.
 */
class Memory {
public:
    static constexpr size_t MEMORY_SIZE = 1024 * 1024;  ///< 1MB address space

    Memory();

    /**
     * @brief Read a byte from memory.
     * @param addr Physical address to read from.
     * @return Byte value, or 0xFF if address is invalid.
     */
    uint8 readByte(PhysicalAddress addr) const;

    /**
     * @brief Read a 16-bit word from memory (little-endian).
     * @param addr Physical address to read from.
     * @return Word value, or 0xFFFF if address is invalid.
     */
    uint16 readWord(PhysicalAddress addr) const;

    /**
     * @brief Write a byte to memory.
     * @param addr Physical address to write to.
     * @param value Byte value to write.
     */
    void writeByte(PhysicalAddress addr, uint8 value);

    /**
     * @brief Write a 16-bit word to memory (little-endian).
     * @param addr Physical address to write to.
     * @param value Word value to write.
     */
    void writeWord(PhysicalAddress addr, uint16 value);

    /**
     * @brief Load binary data into memory.
     * @param binary Vector containing binary data.
     * @param start_addr Starting address to load at.
     */
    void loadBinary(const std::vector<uint8>& binary, PhysicalAddress start_addr = 0);

    /**
     * @brief Read a range of bytes from memory.
     * @param start Starting address.
     * @param length Number of bytes to read.
     * @return Vector containing the requested bytes.
     */
    std::vector<uint8> readRange(PhysicalAddress start, size_t length) const;

    /**
     * @brief Clear all memory to zero.
     */
    void clear();

    /**
     * @brief Set callback for memory write notifications.
     * @param callback Function called on every memory write.
     */
    void setWriteCallback(std::function<void(PhysicalAddress, uint8)> callback) {
        m_write_callback = callback;
    }

    /**
     * @brief Get direct read-only access to memory buffer.
     * @return Pointer to memory data.
     */
    const uint8* data() const { return m_memory.data(); }

    /**
     * @brief Get memory size.
     * @return Size in bytes.
     */
    size_t size() const { return m_memory.size(); }

private:
    std::vector<uint8> m_memory;
    std::function<void(PhysicalAddress, uint8)> m_write_callback;

    bool isValidAddress(PhysicalAddress addr) const {
        return addr < MEMORY_SIZE;
    }
};

} // namespace e2emu
