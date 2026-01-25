#include "test_fixture.h"

TEST_F(EmulatorTest, Movsb) {
    ASSERT_TRUE(loadBinary("movsb"));
    runUntilHalt();

    EXPECT_EQ(emu.getMemory().readByte(0x3000), 0x42);
}

TEST_F(EmulatorTest, Stosb) {
    ASSERT_TRUE(loadBinary("stosb"));
    runUntilHalt();

    EXPECT_EQ(emu.getMemory().readByte(0x3000), 0xAB);
}

TEST_F(EmulatorTest, Lodsb) {
    ASSERT_TRUE(loadBinary("lodsb"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().getReg8(Reg8::AL), 0xCD);
}

TEST_F(EmulatorTest, RepMovsb) {
    ASSERT_TRUE(loadBinary("rep_movsb"));
    runUntilHalt();

    EXPECT_EQ(emu.getMemory().readByte(0x3000), 0x11);
    EXPECT_EQ(emu.getMemory().readByte(0x3001), 0x22);
    EXPECT_EQ(emu.getMemory().readByte(0x3002), 0x33);
    EXPECT_EQ(emu.getCPU().CX, 0);
}
