#include "test_fixture.h"

TEST_F(EmulatorTest, PushPop) {
    ASSERT_TRUE(loadBinary("push_pop"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x1234);
}

TEST_F(EmulatorTest, PushPopMultiple) {
    ASSERT_TRUE(loadBinary("push_pop_multi"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x3333);
    EXPECT_EQ(emu.getCPU().BX, 0x2222);
    EXPECT_EQ(emu.getCPU().CX, 0x1111);
}

TEST_F(EmulatorTest, PushfPopf) {
    ASSERT_TRUE(loadBinary("pushf_popf"));
    runUntilHalt();

    EXPECT_TRUE(emu.getCPU().flags.CF);
}
