<h1 align="center">
  The Ballistic JIT Engine
</h1>

<p align="center"><em>„The world's fastest ARM recompiler“</em></p>

# Overview

This is a rewrite of the dynarmic recompiler, with the goal of fixing its many flaws.

# Immediate Goals

- [X] Support `MOVZ`, `MOVK`, `MOVN` instructions.
- [ ] Support `ADD` instructions.
- [ ] Implement backend.

# Building Ballistic

## Install Dependencies

### macOS

```bash
brew install cmake python3 cmark llvm
```

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake python3 libcmark-dev libclang-dev llvm-dev
```

### Fedora

```bash
sudo dnf install cmake python3 gcc-c++ cmark-devel clang-devel llvm-devel
```

## Configure CMake

```bash
mkdir build
cd build
cmake ..
```

### macOS (If LLVM is not found)

```bash
cmake -DCMAKE_PREFIX_PATH=$(brew --prefix llvm) ..
```
## Build Binaries

```bash
cmake --build .
```

The following executables will be created in the `build/` directory:
*   `libBallistic.a` (Static Library)
*   `ballistic_cli` (Used for Ballistic development)
*   `decoder_cli` (Instruction decoding tool)
*   `cdoc` (Documentation generator)
*   `test_*` (Test suite, run with `ctest`)

See [tools/](tools/) for more information on these executables.

