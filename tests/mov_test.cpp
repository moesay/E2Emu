#include "test_fixture.h"

TEST_F(EmulatorTest, MovRegImm16) {
    ASSERT_TRUE(loadBinary("mov_reg_imm16"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x1234);
    EXPECT_EQ(emu.getCPU().BX, 0x5678);
    EXPECT_EQ(emu.getCPU().CX, 0x9ABC);
    EXPECT_EQ(emu.getCPU().DX, 0xDEF0);
}

TEST_F(EmulatorTest, MovRegImm8) {
    ASSERT_TRUE(loadBinary("mov_reg_imm8"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x3412);
    EXPECT_EQ(emu.getCPU().BX, 0x7856);
}

TEST_F(EmulatorTest, MovRegReg) {
    ASSERT_TRUE(loadBinary("mov_reg_reg"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x1234);
    EXPECT_EQ(emu.getCPU().BX, 0x1234);
    EXPECT_EQ(emu.getCPU().CX, 0x1234);
}

TEST_F(EmulatorTest, MovMemory) {
    ASSERT_TRUE(loadBinary("mov_memory"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().BX, 0xABCD);
    EXPECT_EQ(emu.getMemory().readWord(0x2000), 0xABCD);
}
