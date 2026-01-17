# Tools

This folder holds scritps needed to build Ballistic and standalone programs used for testing Ballistic.

## Standlone Programs

These programs will appear in directory you compile Ballistic with.

### Ballistic CLI

This is used by developers to test Ballistic's translation loop.

### CDoc

This creates rustdoc like documentation for C code. CDoc completely relies on
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

These scripts are solely used for building Ballistic and is called by CMake.

### Generate A64 Table

This script parses the Official ARM Machine Readable Architecture Specification XML files in `spec/` and generates a hash table Ballistic's decoder uses to lookup instructions.
