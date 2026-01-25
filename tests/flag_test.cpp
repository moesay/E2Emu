#include "test_fixture.h"

TEST_F(EmulatorTest, CmpEqual) {
    ASSERT_TRUE(loadBinary("cmp_equal"));
    runUntilHalt();

    EXPECT_TRUE(emu.getCPU().flags.ZF);
    EXPECT_FALSE(emu.getCPU().flags.CF);
}

TEST_F(EmulatorTest, CmpLess) {
    ASSERT_TRUE(loadBinary("cmp_less"));
    runUntilHalt();

    EXPECT_FALSE(emu.getCPU().flags.ZF);
    EXPECT_TRUE(emu.getCPU().flags.CF);
}

TEST_F(EmulatorTest, CmpSign) {
    ASSERT_TRUE(loadBinary("cmp_sign"));
    runUntilHalt();

    EXPECT_TRUE(emu.getCPU().flags.SF);
}

TEST_F(EmulatorTest, Stc) {
    ASSERT_TRUE(loadBinary("stc"));
    runUntilHalt();

    EXPECT_TRUE(emu.getCPU().flags.CF);
}

TEST_F(EmulatorTest, Clc) {
    ASSERT_TRUE(loadBinary("clc"));
    runUntilHalt();

    EXPECT_FALSE(emu.getCPU().flags.CF);
}

TEST_F(EmulatorTest, Cmc) {
    ASSERT_TRUE(loadBinary("cmc"));
    runUntilHalt();

    EXPECT_TRUE(emu.getCPU().flags.CF);
}
