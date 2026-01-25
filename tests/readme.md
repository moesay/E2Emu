# E2Emu Tests

Unit tests for the 8086 emulator using Google Test and NASM-assembled binaries.

## Prerequisites

- **NASM** - The Netwide Assembler for assembling test programs
- **CMake** 3.14+
- **GTest** - fetched automatically by CMake

Install NASM on Ubuntu/Debian:
```bash
sudo apt install nasm
```

## Building and Running Tests

From the project root:

```bash
mkdir build && cd build
cmake -DE2EMU_TEST=ON ..
make
```

Run all tests:
```bash
ctest --output-on-failure
```

Or with verbose output:
```bash
make test_emu_verbose
```

## Adding New Tests

### 1. Write the assembly program

Create a new `.asm` file in `tests/asm/`. All test programs should:

- Start with `BITS 16` and `ORG 0x100`
- End with `hlt` so the emulator knows when to stop

Example (`tests/asm/my_test.asm`):
```asm
BITS 16
ORG 0x100
mov ax, 42
add ax, 8
hlt
```

The build system will automatically assemble it to `my_test.bin`.

### 2. Write the C++ test

Add a test to an existing `*_test.cpp` file or create a new one. Use `loadBinary()` with the name (without extension) and check the results:

```cpp
TEST_F(EmulatorTest, MyTest) {
    ASSERT_TRUE(loadBinary("my_test"));
    runUntilHalt();

    EXPECT_EQ(emu.getCPU().AX, 50);
}
```

If you create a new test file, add it to `E2EMU_TEST_SOURCES` in the root `CMakeLists.txt`.

### 3. Rebuild

```bash
make
ctest -R MyTest
```

## Test Organization

| File | Tests |
|------|-------|
| `mov_test.cpp` | MOV instruction variants |
| `arithmetic_test.cpp` | ADD, SUB, MUL, DIV, INC, DEC, NEG |
| `logic_test.cpp` | AND, OR, XOR, NOT, TEST |
| `shift_test.cpp` | SHL, SHR, SAR, ROL, ROR |
| `stack_test.cpp` | PUSH, POP, PUSHF, POPF |
| `control_flow_test.cpp` | JMP, Jcc, LOOP, CALL, RET |
| `flag_test.cpp` | CMP, STC, CLC, CMC |
| `string_test.cpp` | MOVSB, STOSB, LODSB, REP |
| `misc_test.cpp` | XCHG, LEA, CWD, NOP |
| `addressing_test.cpp` | Addressing modes |
| `integration_test.cpp` | Multi-instruction programs |
| `emulator_state_test.cpp` | Reset, stepping |
| `device_test.cpp` | Ports, VGA |
