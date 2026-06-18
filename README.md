<h1 align="center">
  The Ballistic JIT Engine
</h1>

<p align="center"><em>„The world's fastest ARM recompiler“</em></p>

# Overview

This is a rewrite of the dynarmic recompiler, with the goal of fixing its many flaws.

# Version 1.0 Goals

- [X] Create Tier 1 backend compiler.
- [ ] Create Tier 2 backend compiler.
- [ ] Support `MOVZ`, `MOVK`, `MOVN` instructions on both compilers.
- [ ] Add more peephole optimizations.
- [ ] Have 100% branch coverage.
- [X] Have a config to change Ballistic behavior at runtime.
- [ ] Support Block linking.
- [X] Map ARM flags to x86 flags.
- [ ] Support 128-bit types and x86 SSE/AVX instructions.
- [ ] Add exception handling and recover guest CPU state.
- [ ] Add register spilling.
- [ ] Add Software Page Tables for MMIO and expose the page table memory layout.
- [ ] Handle Guest W^X and Guest RO/RW.
- [ ] Support Guest Write permissions and MMIO write traps for `bal_translate_write_function_t`.
- [ ] Allow the Guest to inform the memory subsystem that a Guest page has changed state.
- [ ] Invalidate JIT caches when Guest memory is modified using `bal_invalidate_git_cache_function_t`.
- [ ] Rewrite `tools/cdoc.c`
- [ ] Rewrite all Python scripts in Lua.
- [ ] Add code examples on how to use a header file like in `bal_x86_sliding_window.h`.
- [ ] Reorganize all functions in alphabetical order in `.c` and `.h` files.
- [ ] Add benchmarks measuring compilation speed compared to other JIT compilers.

# Building Ballistic

## Install Dependencies

### macOS

```bash
brew install cmake python3 llvm
```

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake python3
```

### Fedora

```bash
sudo dnf install cmake python3 gcc-c++
```

## Configure CMake

```bash
mkdir build
cd build
cmake ..
```

## Build Binaries

```bash
cmake --build .
```

The compiled executables will be created in the `build/bin` and `build/lib` directories.

See [tools/](tools/) for more information on these executables.

