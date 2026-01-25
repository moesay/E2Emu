/**
 * @file port_controller.cpp
 * @brief I/O port controller implementation.
 */

#include "port_controller.h"

namespace e2emu {

PortController::PortController() {
}

void PortController::registerDevice(uint16 port_start, uint16 port_end,
                                    std::shared_ptr<IODevice> device) {
    for (uint32 port = port_start; port <= port_end; ++port) {
        m_port_map[static_cast<uint16>(port)] = device;
    }
}

void PortController::unregisterDevice(uint16 port_start, uint16 port_end) {
    for (uint32 port = port_start; port <= port_end; ++port) {
        m_port_map.erase(static_cast<uint16>(port));
    }
}

uint8 PortController::in(uint16 port) {
    uint8 value;

    auto it = m_port_map.find(port);
    if (it != m_port_map.end()) {
        // Device is registered - use device's portIn
        value = it->second->portIn(port);
    } else {
        // No device registered - check cached values first (within same execution session)
        auto cached_it = m_cached_ports.find(port);
        if (cached_it != m_cached_ports.end()) {
            value = cached_it->second;
        } else if (m_interactive_mode && m_port_input_request_callback) {
            // Interactive mode enabled - ask for user input
            value = m_port_input_request_callback(port);
            // Cache the input value for future reads in this session
            m_cached_ports[port] = value;
        } else {
            // Interactive mode disabled - check predefined values
            auto predefined_it = m_predefined_ports.find(port);
            if (predefined_it != m_predefined_ports.end()) {
                value = predefined_it->second;
            } else {
                value = 0xFF;  // Default value for unconnected ports with no predefined value
            }
        }
    }

    // Notify callback if set
    if (m_port_access_callback) {
        m_port_access_callback(port, value, true);
    }

    return value;
}

void PortController::out(uint16 port, uint8 value) {
    auto it = m_port_map.find(port);
    if (it != m_port_map.end()) {
        it->second->portOut(port, value);
    } else {
        // Store value in cached ports (not predefined)
        m_cached_ports[port] = value;
    }

    // Notify callback if set
    if (m_port_access_callback) {
        m_port_access_callback(port, value, false);
    }
}

void PortController::clear() {
    m_port_map.clear();
    m_predefined_ports.clear();
    m_cached_ports.clear();
}

void PortController::clearCachedValues() {
    // Only clear cached values from user input, keep predefined values
    m_cached_ports.clear();
}

void PortController::setDefaultPortValue(uint16 port, uint8 value) {
    // Set predefined port value (from settings)
    m_predefined_ports[port] = value;
}

std::shared_ptr<IODevice> PortController::getDevice(uint16 port) const {
    auto it = m_port_map.find(port);
    if (it != m_port_map.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace e2emu
