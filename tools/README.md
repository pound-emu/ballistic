# Tools

This folder holds scritps needed to build Ballistic and standalone programs used for testing Ballistic.

## Standlone Programs

These programs will appear in directory you compile Ballistic with.

### Decoder CLI

This program is used for decoding ARM64 instructions. The following example shows how to use it:
```bash
./decoder_cli 0b5f1da1 # ADD extended registers
Mnemonic: ADD - Mask: 0x7F200000 - Expected: 0x0B000000
```

### Coverage CLI

This program takes an ARM64 binary file and outputs the 20 most common instructions.

## Scripts

These scripts are solely used for building Ballistic and is called by CMake.

### Generate A64 Table

This script parses the Official ARM Machine Readable Architecture Specification XML files in `spec/` and generates a hash table Ballistic's decoder uses to lookup instructions.
