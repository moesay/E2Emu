<p align="center">
  <a href="https://github.com/moesay/e2emu/">
    <img src="https://github.com/moesay/e2asm/blob/main/e2-logo.png" alt="TheE2Project" width="763" height="169">
  </a>
</p>

<h3 align="center">E2Emu</h3>

<p align="center">
  A cross-platform 8086 emulator with BIOS and DOS interrupt support.
  <br>
  <a href="https://github.com/moesay/e2emu/issues/new?template=bug_report.md">Report bug</a>
  ·
  <a href="https://github.com/moesay/e2emu/issues/new?template=feature_request.md">Request feature</a>
</p>

<p align="center">
      <a href="https://github.com/moesay/e2emu/blob/master/LICENSE" alt="License">
        <img src="https://img.shields.io/github/license/moesay/e2emu" /></a>
      <a href="https://github.com/moesay/e2emu/" alt="Status">
        <img src="https://img.shields.io/badge/Status-WIP-f10" /></a>
      <a href="https://github.com/moesay/e2emu/" alt="Dev Status">
        <img src="https://img.shields.io/badge/Developing-Active-green" /></a>
      <a href="https://github.com/moesay/e2emu/" alt="Repo Size">
        <img src="https://img.shields.io/github/repo-size/moesay/e2emu?label=Repository%20size" /></a>
      <a href="https://github.com/moesay/e2emu/issues/" alt="Issues">
        <img src="https://img.shields.io/github/issues/moesay/e2emu" /></a>
      <a href="https://github.com/moesay/e2emu/pulls/" alt="PRs">
        <img src="https://img.shields.io/github/issues-pr/moesay/e2emu" /></a>
 </p>

## Table of contents

- [History](#history)
- [Why?](#why)
- [Overview](#overview)
- [Quick start](#quick-start)
  - [Linux](#linux)
  - [Windows](#windows)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Bugs and features requests](#bugs-and-features-requests)
- [Copyright and license](#copyright-and-license)

## History

Back in 2019, I started a project called **Elegant86**. The idea was to build an all-in-one 8086 assembly suite. The scope was large and the effort required was significant, so I made a conscious decision to keep things as simple as possible _even if that meant using poor design patterns_ just to maintain momentum and make the workflow easier.

One of the main goals was to build a community around the project, as I believed I would need help to sustain and grow it. However, after about a year, I didn't receive the level of contribution I had expected. At that point, I found myself stuck with architectural decisions that were hard to justify, especially without the community support I had initially planned for. As a result, I decided to rewrite the project from scratch.

In the new design, I reworked the architecture and separated the **assembler** from the **emulator**, making each one a standalone, independent project. In Elegant86, everything was bundled into a single project, which made the system harder to evolve and maintain.

The new project is called E2. The name comes from **Enhanced Elegant**, but it also carries a deeper personal meaning: it represents the initials of my father's name, **ElSayed ElKafafy**. May he rest in peace.

## Why?

E2Emu exists because I'm genuinely interested in the 8086 architecture and wanted to create a modern emulator that others can actually use and build upon. Unlike traditional emulators that are designed purely as standalone tools, E2Emu is built with **integration in mind** from the ground up.

This means you can:
- **Embed it into your projects** - Use E2Emu as a library in your own applications
- **Build on top of it** - Create custom tooling, IDEs, or educational platforms that leverage the emulator
- **Integrate it into larger systems** - The clean architecture and well-defined interfaces make it easy to incorporate into debuggers, development environments, or educational software

Whether you're building a retro computing environment, creating an educational tool for teaching assembly language, or developing a custom debugging solution, E2Emu provides the flexibility and modularity you need.

## Overview

E2Emu is a cross-platform 8086 emulator written in modern C++. It provides accurate emulation of the Intel 8086 processor along with essential BIOS and DOS services. The project follows a modular architecture with distinct components:

- **CPU**: Full 8086 register set with segment:offset addressing (20-bit physical addresses)
- **Memory**: 1MB address space with byte and word access
- **ALU**: Arithmetic, logic, shift, and BCD operations with proper flag handling
- **Decoder**: Instruction decoding with ModR/M byte support
- **VGA Device**: Text mode (80x25) and graphics mode (320x200x256) emulation
- **Port Controller**: I/O port management with device registration
- **Interrupt Controller**: BIOS interrupts (INT 10h, INT 16h) and DOS services (INT 21h)

The emulator is designed with clean separation of concerns, making it easier to understand, maintain, and extend.

## Quick start

### Linux

To build and run E2Emu on Linux:

```bash
# Clone the repository
git clone https://github.com/moesay/e2emu.git
cd e2emu

# Create build directory
mkdir -p build && cd build

# Configure with CMake (with tests enabled)
cmake .. -DE2EMU_TEST=ON

# Build the project
make

# Run the unit tests
ctest --output-on-failure
```

**Requirements:**
- CMake 3.14 or higher
- C++20 compatible compiler (GCC 10+, Clang 10+)
- Git (for fetching GoogleTest dependency)
- NASM (for assembling test binaries)

### Windows

To build E2Emu on Windows:

```powershell
# Clone the repository
git clone https://github.com/moesay/e2emu.git
cd e2emu

# Create build directory
mkdir build
cd build

# Configure with CMake (using Visual Studio generator)
cmake .. -DE2EMU_TEST=ON

# Build the project
cmake --build .

# Run the unit tests
ctest --output-on-failure
```

**Requirements:**
- CMake 3.14 or higher
- Visual Studio 2019 or higher (with C++20 support)
- Git (for fetching GoogleTest dependency)
- NASM (for assembling test binaries)

Alternatively, you can use MinGW-w64 or Clang on Windows following similar steps to the Linux build.

## Documentation

API documentation is generated using Doxygen. To build the documentation locally:

```bash
# Install Doxygen (if not already installed)
# Ubuntu/Debian
sudo apt install doxygen graphviz

# Fedora
sudo dnf install doxygen graphviz

# macOS
brew install doxygen graphviz

# Generate the documentation
doxygen Doxyfile

# Open the documentation in your browser
# Linux
xdg-open docs/html/index.html

# macOS
open docs/html/index.html
```

**Note:** Graphviz is optional but recommended for generating class diagrams and call graphs.

The codebase is also structured to be self-documenting with clear separation of concerns:
- Check the `core/` directory for CPU, Memory, and Emulator classes
- Explore `execution/` for the ALU and instruction decoder
- See `devices/` for VGA and port controller implementations
- Check `interrupts/` for BIOS and DOS interrupt handlers
- Unit tests in the `tests/` directory serve as usage examples

## Contributing

We welcome contributions from developers of all skill levels! For now, the contribution process is straightforward:

**Current guidelines:**
- Ensure your changes pass the build: `make` or `cmake --build .`
- Ensure all unit tests pass: `ctest --output-on-failure`
- Write unit tests for new functionality when applicable

That's it! As long as the build succeeds and tests pass, your contribution is welcome.

**Note:** More detailed contribution guidelines will be added in the future as the project matures. This will include coding standards, commit message conventions, and code review processes.

To get started:
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Run the tests to ensure everything works
5. Commit your changes (`git commit -m 'Add some amazing feature'`)
6. Push to your branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

## Bugs and features requests

Found a bug or have an idea for a new feature? We'd love to hear from you!

- **Bug reports**: [Create a bug report](https://github.com/moesay/e2emu/issues/new?template=bug_report.md)
- **Feature requests**: [Request a feature](https://github.com/moesay/e2emu/issues/new?template=feature_request.md)

Before creating a new issue, please search existing issues to avoid duplicates. When reporting bugs, include as much detail as possible:
- Your operating system and version
- Compiler version
- Steps to reproduce the issue
- Expected vs actual behavior
- Any relevant code snippets or binary files

## Copyright and license

E2Emu is licensed under the **GNU General Public License v3.0 (GPL-3.0)**.

This means you are free to:
- Use the software for any purpose
- Change the software to suit your needs
- Share the software with your friends and neighbors
- Share the changes you make

Under the condition that:
- You share your modifications under the same GPL-3.0 license
- You include the original copyright notice

For the complete license text, see the [LICENSE](https://github.com/moesay/e2emu/blob/master/LICENSE) file in the repository.

---

<p align="center">Made with love, determination, and a passion for the 8086 processor</p>
<p align="center">Copyright (C) 2019-2026. The E2Emu Contributors</p>
