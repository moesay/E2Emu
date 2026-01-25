#pragma once

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "E2Emu/core/emulator.h"
#include "E2Emu/core/cpu.h"
#include "E2Emu/core/memory.h"
#include "E2Emu/devices/vga_device.h"
#include "E2Emu/devices/port_controller.h"

using namespace e2emu;

class EmulatorTest : public ::testing::Test {
protected:
    Emulator emu;

    void SetUp() override {
        emu.reset();
    }

    bool loadBinary(const std::string& name, uint16 cs = 0x0000, uint16 ip = 0x0100) {
        std::filesystem::path binPath = std::filesystem::path(TEST_ASM_DIR) / (name + ".bin");

        std::ifstream file(binPath, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open: " << binPath << std::endl;
            return false;
        }

        std::vector<uint8> binary((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
        emu.loadProgram(binary, cs, ip);
        return true;
    }

    size_t runUntilHalt(size_t max = 10000) {
        return emu.run(max);
    }
};
