All memory will be allocated from a contiguous memory arena before the pass begins. No `malloc` calls inside the loop.

## Variable Design

```c
// We implement a tag system to track which operands are variables and constant.
#define TAG_CONSTANT     0x80000000

// Ref: Chapter 3.1.3
typedef struct
{
    type_t type; // TYPE_INT, TYPE_FLOAT

    // 10: x = 4        Defines x so reachingDef = 1 (x₁)
    // 20: y = x + 1    Only uses x so reachingDef is still 1.
    // 30: x = 5        Redefines x so reachingDef = 2 (x₂).
    int reaching_definition;
} original_variable_t;

// This array maps the source code definition to its SSA version.
// It is the frontend's job to populate this array.
//
// Example:
//
// Instruction 100: x = y + z
//
// If `x` is at index 5, original_variables[5].reachingDef = 100.
// `x` maps to instruction 100.
original_variable_t original_variables[???];

typedef struct
{
    int original_variable_id;

    // If < TAG_CONSTANT: It's an index into the instructions array.
    // If >= TAG_CONSTANT: It's an index into the constants array (with the
    // high bit masked off).
    int defining_instruction;
} ssa_version_t

// This array tracks variables and constants produced by the SSA.
// 
// This example shows the logic for tracking constants:
//
// 10: x1 = 5 
// ssa_versions[x1].original_var_id = 0 // Constants have no ID.
// ssa_versions[x1].defining_instruction = 10 | TAG_CONSTANT
//
// 20: y1 = x1 + 5
// ssa_versions[y1].original_var_id = 0
// ssa_versions[y1].defining_instruction = 20 | TAG_CONSTANT
// 
// 30: x2 = 20
// ssa_versions[x2].original_var_id = 0
// ssa_versions[x2].defining_instruction = 30 | TAG_CONSTANT
ssa_version_t ssa_versions[???];
```
## Constants Design

An open addressing hash map will need to be created to map constants to their index for `constants[]`.
The hashmap will be used by the frontend and IR. `constants[]` should only be used by the backend.

```c
typedef enum { CONST_INT64, CONST_DOUBLE } const_type_t;

typedef struct {
    const_type_t type;
    union {
        uint64_t i64;
        double   f64;
        // etc...
    } value;

} constant_t;

// The frontend is responsible for populating this array.
constant_t constants[???];
```

## Instruction Design

```c
// We implement a tag system to track which operands are variables and constant.
#define TAG_SSA_VERSION  0x00000000
#define TAG_CONSTANT     0x80000000
#define TAG_MASK         0xC0000000

// Struct should be at most 32 bytes so 2 can fit on one cache line.
typedef struct
{
    uint16_t opcode;

    // Bitfields.
    uint16_t flags;

    // SSA versions
    // x3 = y1 + z2
    uint32_t definition; // x3
    uint32_t operand1;   // y1
    uint32_t operand2;   // z2
} instruction_t;

// An example of how variable renaming should work. This does not apply to
// constants. We will focus on the variable `a`. Lets assume variable `a` is
// stored in original_variables[5].
//
// 10: a1 = ???
// original_variables[5].reaching_definition = 1;
// ssa_versions[1].defining_instruction = 10;
// ssa_versions[1].original_variable_id = 5;
//
// 20: b1 = a1 + ???;
// instructions[20].operand1 = original_variables[5].reaching_definition;
// 
// 30: c1 = a1 + b1;
// instructions[30].operand1 = original_variables[5].reaching_definition;
instruction_t* instructions[???];
```

## Instruction Flags Design

### Layout Strategy

```text
15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
|| |--------------|  |------| |---------------|
R      Class          Width      Global Flags
      Specific
```

### 1. Global Flags [5:0]
These apply to *every* instruction type.

1. `SIDE_EFFECT` Bit[0]

Dont delete this instruction. Used for Dead Code Elimination.

2. `COMMUTATIVE` Bit[1]

Order doesn't matter. `ADD x, 5` == `ADD 5, x` Used for Global value numbering.

3. `MAY_TRAP` Bit[2]

Dont move the instruction aggressively. Set this for `DIV` (div by zero) and `LDR` (segfault). Used for Loop Invariant Code Motion.

4. `READS_MEMORY` Bit[3]

Instruction depends on the heap state.

5. `TERMINATOR` Bit[4]

Instruction ends the basic block.

6. `SPILL` Bit[5]

Instruction was created by the register allocator.

### 2. Width [8:6]

Every instruction must set these bits accordingly.

| Encoding | Mnemonic | Description |
| :--- | :--- | :--- |
| `000` | **DEF** | Default / Void / Context-implied width. |
| `001` | **B** | Byte (8-bit integer). |
| `010` | **H** | Half-word (16-bit integer). |
| `011` | **W** | Word (32-bit integer / float). |
| `100` | **X** | Double-word (64-bit integer / double). |
| `101` | **Q** | Quad-word (128-bit SIMD vector). |
| `110` | **PTR** | Object Reference / Pointer (GC tracked). |
| `111` | - | *Reserved.* |

### 3. Class-Specific Encodings [14:9]
We redefine these bits based on the Opcode.

#### Shift and Rotate (`LSL`, `LSR`, `ASR`, `ROR`)

Use when the shift amount is a compile-time constant.

```text
15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
|| |--------------|  |------| |---------------|
R    Shift Amount     Width      Global Flags
```

#### Vector Element (`EXT`, `INS`, `DUP`)

Used for SIMD lane manipulation where the index must be an immediate.

```text
15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
|------| |---------| |------| |---------------|
   R        Index     Width     Global Flags
```

<**Index**> Vector lane index (0-15).
<**Width**> Must be set to `101` for 128-bit operations.

#### Branch and Compare (`B.cond`, `CSET`, `CSEL`)

Used for operations dependant on the PSTATE condition flags.

```text
15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
|------| |---------| |------| |---------------|
   R      Condition    Width     Global Flags
```

| Encoding | Mnemonic | Meaning |
| :--- | :--- | :--- |
| `0000` | **EQ** | Equal |
| `0001` | **NE** | Not Equal |
| `0010` | **CS / HS** | Carry Set / Unsigned Higher or Same |
| `0011` | **CC / LO** | Carry Clear / Unsigned Lower |
| TODO | TODO | TODO | 
| `1110` | **AL** | Always |

### 4. Access Macros

Direct bitwise manipulation of flags is discouraged outside of IR Builder. Use these macros instead:

```c
// Global Flags
#define IS_SIDE_EFFECT(f)  ((f) & 0x1)
#define IS_COMMUTATIVE(f)  ((f) & 0x2)
#define IS_TRAPPING(f)     ((f) & 0x4)

// Width Extraction
#define GET_WIDTH(f)       (((f) >> 6) & 0x7)

// Opcode Dependant Bits
#define GET_SHIFT_VALUET(f)       (((f) >> 9) & 0x3F)
#define GET_LANE(f)        (((f) >> 6) & 0xF)
#define GET_COND(f)        (((f) >> 6) & 0xF)
// etc...
```
