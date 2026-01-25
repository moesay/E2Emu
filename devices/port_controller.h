#pragma once

/**
 * @file port_controller.h
 * @brief I/O port management for the 8086 emulator.
 */

#include "../core/types.h"
#include <map>
#include <memory>
#include <functional>

namespace e2emu {

/**
 * @brief Abstract base class for I/O devices.
 *
 * Devices that handle I/O port access must inherit from this class
 * and implement the port read/write methods.
 */
class IODevice {
public:
    virtual ~IODevice() = default;

    /**
     * @brief Handle IN instruction (read from port).
     * @param port Port number being read.
     * @return Byte value to return to CPU.
     */
    virtual uint8 portIn(uint16 port) = 0;

    /**
     * @brief Handle OUT instruction (write to port).
     * @param port Port number being written.
     * @param value Byte value being written.
     */
    virtual void portOut(uint16 port, uint8 value) = 0;

    /**
     * @brief Get device name for debugging.
     * @return Device name string.
     */
    virtual std::string getName() const = 0;
};

/// Callback for port access events (for UI/debugging)
using PortAccessCallback = std::function<void(uint16 port, uint8 value, bool is_input)>;

/// Callback for requesting input from user (synchronous, returns value)
using PortInputRequestCallback = std::function<uint8(uint16 port)>;

/**
 * @brief I/O port controller managing the 64KB port address space.
 *
 * Routes IN/OUT instructions to registered devices or handles
 * unregistered ports with caching and user input callbacks.
 */
class PortController {
public:
    PortController();

    /**
     * @brief Register a device for a range of ports.
     * @param port_start First port number.
     * @param port_end Last port number (inclusive).
     * @param device Device to handle these ports.
     */
    void registerDevice(uint16 port_start, uint16 port_end, std::shared_ptr<IODevice> device);

    /**
     * @brief Unregister device from a port range.
     * @param port_start First port number.
     * @param port_end Last port number (inclusive).
     */
    void unregisterDevice(uint16 port_start, uint16 port_end);

    /**
     * @brief Read from I/O port (IN instruction).
     * @param port Port number.
     * @return Byte value read.
     */
    uint8 in(uint16 port);

    /**
     * @brief Write to I/O port (OUT instruction).
     * @param port Port number.
     * @param value Byte value to write.
     */
    void out(uint16 port, uint8 value);

    /**
     * @brief Clear all device registrations and cached values.
     */
    void clear();

    /**
     * @brief Clear only cached port values (keep device registrations).
     */
    void clearCachedValues();

    /**
     * @brief Set default value for a specific port.
     * @param port Port number.
     * @param value Default value to return when read.
     */
    void setDefaultPortValue(uint16 port, uint8 value);

    /**
     * @brief Get device registered for a port.
     * @param port Port number.
     * @return Device pointer, or nullptr if no device registered.
     */
    std::shared_ptr<IODevice> getDevice(uint16 port) const;

    /**
     * @brief Set callback for port access notifications.
     * @param callback Function called on every IN/OUT.
     */
    void setPortAccessCallback(PortAccessCallback callback) {
        m_port_access_callback = callback;
    }

    /**
     * @brief Set callback for user input requests.
     * @param callback Function called when IN needs user input.
     */
    void setPortInputRequestCallback(PortInputRequestCallback callback) {
        m_port_input_request_callback = callback;
    }

    /**
     * @brief Enable/disable interactive input mode.
     * @param enabled If true, prompt user for unregistered port reads.
     */
    void setInteractiveMode(bool enabled) {
        m_interactive_mode = enabled;
    }

    /**
     * @brief Check if interactive mode is enabled.
     * @return true if interactive mode is on.
     */
    bool isInteractiveMode() const {
        return m_interactive_mode;
    }

private:
    std::map<uint16, std::shared_ptr<IODevice>> m_port_map;
    std::map<uint16, uint8> m_predefined_ports;
    std::map<uint16, uint8> m_cached_ports;
    PortAccessCallback m_port_access_callback;
    PortInputRequestCallback m_port_input_request_callback;
    bool m_interactive_mode = true;
};

} // namespace e2emu
