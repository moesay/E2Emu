#include "test_fixture.h"

TEST_F(EmulatorTest, Xchg) {
    ASSERT_TRUE(loadBinary("xchg"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x2222);
    EXPECT_EQ(emu.getCPU().BX, 0x1111);
}

TEST_F(EmulatorTest, Lea) {
    ASSERT_TRUE(loadBinary("lea"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().BX, 15);
}

TEST_F(EmulatorTest, CwdPositive) {
    ASSERT_TRUE(loadBinary("cwd_pos"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().DX, 0x0000);
}

TEST_F(EmulatorTest, CwdNegative) {
    ASSERT_TRUE(loadBinary("cwd_neg"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().DX, 0xFFFF);
}

TEST_F(EmulatorTest, Nop) {
    ASSERT_TRUE(loadBinary("nop"));
    runUntilHalt();

    EXPECT_EQ(emu.getState(), ExecutionState::HALTED);
}
