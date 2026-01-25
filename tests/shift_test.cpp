#include "test_fixture.h"

TEST_F(EmulatorTest, Shl) {
    ASSERT_TRUE(loadBinary("shl"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0002);
}

TEST_F(EmulatorTest, Shr) {
    ASSERT_TRUE(loadBinary("shr"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0001);
}

TEST_F(EmulatorTest, Sar) {
    ASSERT_TRUE(loadBinary("sar"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0xC000);
}

TEST_F(EmulatorTest, Rol) {
    ASSERT_TRUE(loadBinary("rol"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0003);
    EXPECT_TRUE(emu.getCPU().flags.CF);
}

TEST_F(EmulatorTest, Ror) {
    ASSERT_TRUE(loadBinary("ror"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x8000);
    EXPECT_TRUE(emu.getCPU().flags.CF);
}

TEST_F(EmulatorTest, ShlByCL) {
    ASSERT_TRUE(loadBinary("shl_cl"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0010);
}
