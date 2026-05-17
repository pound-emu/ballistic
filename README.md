<h1 align="center">
  The Ballistic JIT Engine
</h1>

<p align="center"><em>„The world's fastest ARM recompiler“</em></p>

# Overview

This is a rewrite of the dynarmic recompiler, with the goal of fixing its many flaws.

# Immediate Goals

- [ ] Create Tier 1 backend compiler.
- [ ] Create Tier 2 backend compiler.
- [ ] Support `MOVZ`, `MOVK`, `MOVN` instructions on both compilers.
- [ ] Add more peephole optimizations.
- [ ] Have 100% branch coverage.
- [ ] Have a config to change Ballistic behavior at runtime.

# Building Ballistic

## Install Dependencies

### macOS

```bash
brew install cmake python3 llvm
```

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake python3 libclang-dev llvm-dev
```

### Fedora

```bash
sudo dnf install cmake python3 gcc-c++ clang-devel llvm-devel
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

* `libBallistic.a` (Static Library)
* `ballistic_cli` (Used for Ballistic development)
* `decoder_cli` (Instruction decoding tool)
* `cdoc` (Documentation generator)
* `test_*` (Test suite, run with `ctest`)

See [tools/](tools/) for more information on these executables.

