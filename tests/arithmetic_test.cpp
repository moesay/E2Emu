#include "test_fixture.h"

TEST_F(EmulatorTest, Add) {
    ASSERT_TRUE(loadBinary("add"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0030);
}

TEST_F(EmulatorTest, AddWithCarry) {
    ASSERT_TRUE(loadBinary("add_carry"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0000);
    EXPECT_TRUE(emu.getCPU().flags.CF);
    EXPECT_TRUE(emu.getCPU().flags.ZF);
}

TEST_F(EmulatorTest, Sub) {
    ASSERT_TRUE(loadBinary("sub"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0020);
}

TEST_F(EmulatorTest, SubWithBorrow) {
    ASSERT_TRUE(loadBinary("sub_borrow"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0xFFFF);
    EXPECT_TRUE(emu.getCPU().flags.CF);
    EXPECT_TRUE(emu.getCPU().flags.SF);
}

TEST_F(EmulatorTest, IncDec) {
    ASSERT_TRUE(loadBinary("inc_dec"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0100);
    EXPECT_EQ(emu.getCPU().BX, 0x00FF);
}

TEST_F(EmulatorTest, Neg) {
    ASSERT_TRUE(loadBinary("neg"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0xFFFF);
}

TEST_F(EmulatorTest, MulByte) {
    ASSERT_TRUE(loadBinary("mul_byte"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 50);
}

TEST_F(EmulatorTest, MulWord) {
    ASSERT_TRUE(loadBinary("mul_word"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 20000);
    EXPECT_EQ(emu.getCPU().DX, 0);
}

TEST_F(EmulatorTest, DivByte) {
    ASSERT_TRUE(loadBinary("div_byte"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().getReg8(Reg8::AL), 7);
    EXPECT_EQ(emu.getCPU().getReg8(Reg8::AH), 1);
}

TEST_F(EmulatorTest, DivWord) {
    ASSERT_TRUE(loadBinary("div_word"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 33);
    EXPECT_EQ(emu.getCPU().DX, 10);
}

TEST_F(EmulatorTest, Adc) {
    ASSERT_TRUE(loadBinary("adc"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x0016);
}

TEST_F(EmulatorTest, Sbb) {
    ASSERT_TRUE(loadBinary("sbb"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x001A);
}
