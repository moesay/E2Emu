#include "test_fixture.h"

TEST_F(EmulatorTest, DirectAddressing) {
    ASSERT_TRUE(loadBinary("direct_addr"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x1234);
}

TEST_F(EmulatorTest, RegisterIndirect) {
    ASSERT_TRUE(loadBinary("reg_indirect"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x5678);
}

TEST_F(EmulatorTest, RegisterDisplacement) {
    ASSERT_TRUE(loadBinary("reg_disp"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0xABCD);
}

TEST_F(EmulatorTest, BaseIndex) {
    ASSERT_TRUE(loadBinary("base_index"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0xFEDC);
}

TEST_F(EmulatorTest, SegmentOffset) {
  ASSERT_TRUE(loadBinary("seg_offset"));
  runUntilHalt();

  EXPECT_EQ(emu.getMemory().readWord(0x10020), 0x1234);
}
