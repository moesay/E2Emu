#include "test_fixture.h"

TEST_F(EmulatorTest, OutImmediate) {
    emu.getPortController().setInteractiveMode(false);

    ASSERT_TRUE(loadBinary("out_imm"));
    runUntilHalt();

    uint8 value = emu.getPortController().in(0x60);
    EXPECT_EQ(value, 0x42);
}

TEST_F(EmulatorTest, OutDx) {
    emu.getPortController().setInteractiveMode(false);

    ASSERT_TRUE(loadBinary("out_dx"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().DX, 0x3D4);
}

TEST_F(EmulatorTest, InImmediate) {
    emu.getPortController().setInteractiveMode(false);
    emu.getPortController().setDefaultPortValue(0x60, 0x7F);

    ASSERT_TRUE(loadBinary("in_imm"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().getReg8(Reg8::BL), 0x7F);
}

TEST_F(EmulatorTest, InDx) {
    emu.getPortController().setInteractiveMode(false);

    ASSERT_TRUE(loadBinary("in_dx"));
    runUntilHalt();

    uint8 value = emu.getCPU().getReg8(Reg8::BL);
    EXPECT_TRUE(value == 0x00 || value == 0x08);
}

TEST_F(EmulatorTest, PortReadback) {
    emu.getPortController().setInteractiveMode(false);

    ASSERT_TRUE(loadBinary("port_readback"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().getReg8(Reg8::BL), 0xAB);
}

TEST_F(EmulatorTest, PortDefaultValue) {
    emu.getPortController().setInteractiveMode(false);

    uint8 value = emu.getPortController().in(0x99);
    EXPECT_EQ(value, 0xFF);

    emu.getPortController().setDefaultPortValue(0x99, 0x55);
    value = emu.getPortController().in(0x99);
    EXPECT_EQ(value, 0x55);
}

TEST_F(EmulatorTest, PortClearCachedValues) {
    emu.getPortController().setInteractiveMode(false);

    emu.getPortController().out(0x77, 0xAA);
    EXPECT_EQ(emu.getPortController().in(0x77), 0xAA);

    emu.getPortController().clearCachedValues();
    emu.getPortController().setDefaultPortValue(0x77, 0x11);

    EXPECT_EQ(emu.getPortController().in(0x77), 0x11);
}

TEST_F(EmulatorTest, VgaCursorPosition) {
    emu.getPortController().setInteractiveMode(false);

    ASSERT_TRUE(loadBinary("vga_cursor"));
    runUntilHalt();

    int x, y;
    emu.getVGA().getCursorPosition(x, y);

    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 5);
}

TEST_F(EmulatorTest, VgaTextWrite) {
    emu.getPortController().setInteractiveMode(false);

    ASSERT_TRUE(loadBinary("vga_text_write"));
    runUntilHalt();

    emu.getVGA().syncBufferFromMemory();

    EXPECT_EQ(emu.getVGA().getChar(0, 0), 'H');
    EXPECT_EQ(emu.getVGA().getAttribute(0, 0), 0x07);
    EXPECT_EQ(emu.getVGA().getChar(1, 0), 'i');
    EXPECT_EQ(emu.getVGA().getAttribute(1, 0), 0x07);
}

TEST_F(EmulatorTest, VgaTextRead) {
    emu.getPortController().setInteractiveMode(false);

    ASSERT_TRUE(loadBinary("vga_text_read"));
    runUntilHalt();

    EXPECT_EQ(emu.getMemory().readByte(0xB8000), 'X');
    EXPECT_EQ(emu.getMemory().readByte(0xB8001), 0x1F);
    EXPECT_EQ(emu.getCPU().ES, 0xB800);
    EXPECT_EQ(emu.getCPU().getReg8(Reg8::AL), 'X');
    EXPECT_EQ(emu.getCPU().getReg8(Reg8::AH), 0x1F);
}

TEST_F(EmulatorTest, VgaPutChar) {
    emu.getVGA().putChar(5, 3, 'A', 0x0F);

    EXPECT_EQ(emu.getVGA().getChar(5, 3), 'A');
    EXPECT_EQ(emu.getVGA().getAttribute(5, 3), 0x0F);

    PhysicalAddress addr = VGADevice::TEXT_BUFFER_ADDR + (3 * 80 + 5) * 2;
    EXPECT_EQ(emu.getMemory().readByte(addr), 'A');
    EXPECT_EQ(emu.getMemory().readByte(addr + 1), 0x0F);
}

TEST_F(EmulatorTest, VgaClearScreen) {
    emu.getVGA().putChar(0, 0, 'X', 0xFF);
    emu.getVGA().putChar(10, 5, 'Y', 0xFF);

    emu.getVGA().clearScreen();

    EXPECT_EQ(emu.getVGA().getChar(0, 0), ' ');
    EXPECT_EQ(emu.getVGA().getAttribute(0, 0), 0x07);
    EXPECT_EQ(emu.getVGA().getChar(10, 5), ' ');

    int x, y;
    emu.getVGA().getCursorPosition(x, y);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
}

TEST_F(EmulatorTest, VgaTeletypeOutput) {
    emu.getVGA().clearScreen();
    emu.getVGA().teletypeOutput('H');
    emu.getVGA().teletypeOutput('i');

    EXPECT_EQ(emu.getVGA().getChar(0, 0), 'H');
    EXPECT_EQ(emu.getVGA().getChar(1, 0), 'i');

    int x, y;
    emu.getVGA().getCursorPosition(x, y);
    EXPECT_EQ(x, 2);
    EXPECT_EQ(y, 0);
}

TEST_F(EmulatorTest, VgaTeletypeNewline) {
    emu.getVGA().clearScreen();
    emu.getVGA().teletypeOutput('A');
    emu.getVGA().teletypeOutput('\n');
    emu.getVGA().teletypeOutput('B');

    EXPECT_EQ(emu.getVGA().getChar(0, 0), 'A');
    EXPECT_EQ(emu.getVGA().getChar(0, 1), 'B');

    int x, y;
    emu.getVGA().getCursorPosition(x, y);
    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 1);
}

TEST_F(EmulatorTest, VgaVideoMode) {
    EXPECT_EQ(emu.getVGA().getVideoMode(), VGADevice::VideoMode::TEXT_80x25_16COLOR);

    emu.getVGA().setVideoMode(VGADevice::VideoMode::GRAPHICS_320x200_256COLOR);
    EXPECT_EQ(emu.getVGA().getVideoMode(), VGADevice::VideoMode::GRAPHICS_320x200_256COLOR);
}

TEST_F(EmulatorTest, VgaPixelOperations) {
    emu.getVGA().setVideoMode(VGADevice::VideoMode::GRAPHICS_320x200_256COLOR);

    emu.getVGA().putPixel(100, 50, 15);
    EXPECT_EQ(emu.getVGA().getPixel(100, 50), 15);

    PhysicalAddress addr = VGADevice::GFX_BUFFER_ADDR + (50 * 320 + 100);
    EXPECT_EQ(emu.getMemory().readByte(addr), 15);
}

TEST_F(EmulatorTest, VgaPortCrtcIndex) {
    emu.getPortController().setInteractiveMode(false);

    emu.getPortController().out(0x3D4, 0x0A);

    EXPECT_EQ(emu.getPortController().in(0x3D4), 0x0A);
}

TEST_F(EmulatorTest, VgaStatusRegisterToggle) {
    emu.getPortController().setInteractiveMode(false);

    uint8 first = emu.getPortController().in(0x3DA);
    uint8 second = emu.getPortController().in(0x3DA);

    EXPECT_NE(first, second);
    EXPECT_TRUE((first == 0x00 && second == 0x08) || (first == 0x08 && second == 0x00));
}

TEST_F(EmulatorTest, VgaBoundsCheck) {
    emu.getVGA().putChar(-1, 0, 'X', 0x07);
    emu.getVGA().putChar(80, 0, 'X', 0x07);
    emu.getVGA().putChar(0, 25, 'X', 0x07);

    EXPECT_EQ(emu.getVGA().getChar(-1, 0), 0);
    EXPECT_EQ(emu.getVGA().getChar(80, 0), 0);
    EXPECT_EQ(emu.getVGA().getAttribute(0, 25), 0);
}
