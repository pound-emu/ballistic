# Tools

This folder holds scripts needed to build Ballistic and standalone programs used for testing Ballistic.

## Standalone Programs

These programs will appear in the directory you compile Ballistic in.

### Ballistic CLI

This is used by developers to test Ballistic's translation loop.

### CDoc

This creates rustdoc-like documentation for C code. CDoc completely relies on
Clang and LLVM so only Unix systems are supported. Builing CDoc on Windows is
not supported at this time.

```bash
# Parses all header files in `include/` and output HTML files to `docs/cdoc`.
./cdoc docs/cdoc include/*
Using Clang headers: /usr/lib/clang/21/include
Parsing ../include/bal_attributes.h...
Parsing ../include/bal_decoder.h...
Parsing ../include/bal_engine.h...
Parsing ../include/bal_errors.h...
Parsing ../include/bal_memory.h...
Parsing ../include/bal_platform.h...
Parsing ../include/bal_types.h...
Generating HTML in '../docs/cdoc'...
Done! Open ../docs/cdoc/index.html
```

**Disclaimer**: I have absolutely zero motivation to create a documentation
generator so `cdoc.c` is made completely with AI. The code is messy but the
generated HTML files look beautiful.

### Coverage CLI

This program takes an ARM64 binary file and outputs the 20 most common instructions.

### Decoder CLI

This program is used for decoding ARM64 instructions. The following example shows how to use it:
```bash
./decoder_cli 0b5f1da1 # ADD extended registers
Mnemonic: ADD - Mask: 0x7F200000 - Expected: 0x0B000000
```

## Scripts

These scripts are solely used for building Ballistic and are called by CMake.

### Generate A64 Table

This script parses the Official ARM Machine Readable Architecture Specification XML files in `spec/` and generates a hash table that's used by Ballistic's decoder to lookup instructions.

### Doctest

This script extracts, compiles, and validates code examples written in the documentation. This replicates Rust's doctest feature, where users can embed code in documentation using markdown. This script can be run in the `build/` directory using `ctest --verbose -R DocTest`.
