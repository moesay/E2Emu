#include "test_fixture.h"

TEST_F(EmulatorTest, And) {
    ASSERT_TRUE(loadBinary("and"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0F00);
}

TEST_F(EmulatorTest, Or) {
    ASSERT_TRUE(loadBinary("or"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0FF0);
}

TEST_F(EmulatorTest, Xor) {
    ASSERT_TRUE(loadBinary("xor"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0000);
    EXPECT_TRUE(emu.getCPU().flags.ZF);
}

TEST_F(EmulatorTest, Not) {
    ASSERT_TRUE(loadBinary("not"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0xFF00);
}

TEST_F(EmulatorTest, Test) {
    ASSERT_TRUE(loadBinary("test"));
    runUntilHalt();

    EXPECT_TRUE(emu.getCPU().flags.ZF);
    EXPECT_EQ(emu.getCPU().AX, 0x00FF);
}
