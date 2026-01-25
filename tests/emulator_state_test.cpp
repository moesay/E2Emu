#include "test_fixture.h"

TEST_F(EmulatorTest, ResetState) {
    ASSERT_TRUE(loadBinary("mov_reg_imm16"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 0x1234);

    emu.reset();

    EXPECT_EQ(emu.getCPU().AX, 0);
    EXPECT_EQ(emu.getState(), ExecutionState::HALTED);
}

TEST_F(EmulatorTest, StepExecution) {
    ASSERT_TRUE(loadBinary("nop"));

    emu.step();
    emu.step();
    emu.step();
    emu.step();

    EXPECT_EQ(emu.getState(), ExecutionState::HALTED);
}
