#include "test_fixture.h"

TEST_F(EmulatorTest, JmpForward) {
    ASSERT_TRUE(loadBinary("jmp_forward"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0001);
}

TEST_F(EmulatorTest, JmpBackward) {
    ASSERT_TRUE(loadBinary("jmp_backward"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 3);
    EXPECT_EQ(emu.getCPU().CX, 0);
}

TEST_F(EmulatorTest, Je) {
    ASSERT_TRUE(loadBinary("je"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().BX, 2);
}

TEST_F(EmulatorTest, Jl) {
    ASSERT_TRUE(loadBinary("jl"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().BX, 2);
}

TEST_F(EmulatorTest, Loop) {
    ASSERT_TRUE(loadBinary("loop"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 50);
}

TEST_F(EmulatorTest, CallRet) {
    ASSERT_TRUE(loadBinary("call_ret"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 3);
}

TEST_F(EmulatorTest, NestedCalls) {
    ASSERT_TRUE(loadBinary("nested_calls"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 111);
}
