<h1 align="center">
  The Ballistic JIT Engine
</h1>

<p align="center"><em>„The world's fastest ARM recompiler“</em></p>

# Overview

This is a rewrite of the dynarmic recompiler, with the goal of fixing its many flaws.

# CPU Cache Constraints

Ballistic is being designed to have an extremely low Cache Footprint:

1. L1d: 4 KB
2. L2: 30 KB

# Version 1.0 Goals

- [X] Create Tier 1 backend compiler.
- [ ] Create Tier 2 backend compiler.
- [ ] Support `MOVZ`, `MOVK`, `MOVN` instructions on both compilers.
- [ ] Treat `PACISAP` and `AUTISAP` as `NOP` then strip the upper bits of `X30`.
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
- [ ] Trap cache maintanence instructions then invalidate the block cache at that GVA.
- [ ] Track multiple address spaces with ASIDs.
- [ ] Handle memory aliasing and cache attributes (CPU: Cached, GPU: Cached/Write-Combining)
- [ ] Rewrite `tools/cdoc.c`
- [X] Rewrite all Python scripts in Lua.
- [ ] Add code examples on how to use a header file like in `bal_x86_sliding_window.h`.
- [ ] Reorganize all functions in alphabetical order in `.c` and `.h` files.
- [ ] Add benchmarks measuring compilation speed compared to other JIT compilers.

# Building Ballistic

Ballistic actively supports `clang` and `clang-cl` only.

## Install Dependencies

### macOS

```bash
brew install cmake luajit
```

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake luajit
```

### Fedora

```bash
sudo dnf install cmake clang luajit 
```

### Windows

1. Install [Choco](https://chocolatey.org/install).
2. Run the following commands in PowerShell administrator mode:

```
# Allow running local PowerShell scripts 
Set-ExecutionPolicy RemoteSigned

./tools/ci/setup_dependencies.ps1
```

## Configure CMake

```bash
cmake --preset release
```

## Build Binaries

```bash
cmake --build --preset release
```

# Usage

```c
#include "bal_engine.h"
#include "bal_memory.h"
#include "bal_log.h"
#include <stdlib.h>
#include <string.h>

// Setup logging and default system allocator.
bal_logger_init_default();
bal_allocator_t allocator = {0};
bal_allocator_default_init(&allocator);

// Allocate guest memory and setup a flat 1:1 translation interface,
size_t guest_memory_size = 4096;
uint32_t *guest_memory = allocator.allocate(allocator.context, 16, guest_memory_size);
memset(guest_memory, 0, guest_memory_size);

bal_memory_interface_t memory_interface = {0};
bal_flat_translation_interface_init(&allocator, &memory_interface, guest_memory,
guest_memory_size);

// Initialize CPU and Engine.
bal_cpu_t cpu = {0};
bal_engine_t engine = {0};
bal_engine_init(&engine, &cpu, &allocator, &memory_interface);

guest_memory[0] = 0xD2800540; // MOVZ X0, #42
guest_memory[1] = 0xD65F03C0; // RET

cpu.pc = 0;
cpu.x[30] = BAL_ENGINE_SENTINEL; // Triggers a safe exit when RET is executed.

// Execute the JIT compiled block.
bal_engine_run_thread(&engine);

// cpu.x[0] now contains 42.

// Cleanup.
bal_engine_destroy(&engine);
bal_flat_translation_interface_destroy(&allocator, &memory_interface);
allocator.free(allocator.context, guest_memory, guest_memory_size);
```
