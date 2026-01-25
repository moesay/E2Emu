#include "test_fixture.h"

TEST_F(EmulatorTest, Fibonacci) {
    ASSERT_TRUE(loadBinary("fibonacci"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().BX, 34);
}

TEST_F(EmulatorTest, BubbleSort) {
    ASSERT_TRUE(loadBinary("bubble_sort"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 10);
    EXPECT_EQ(emu.getCPU().BX, 20);
    EXPECT_EQ(emu.getCPU().DX, 30);
}
