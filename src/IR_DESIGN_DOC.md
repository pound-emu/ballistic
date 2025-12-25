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

// This struct is designed for a slot-based renaming algorithm.
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

## Phi Nodes

```c
typedef struct
{
    // The SSA variable defined by this Phi
    uint32_t definition;

    // Index into the phi_operands pool
    // Phi functions appear at the start of a basic block and execute in parallel.
    // The length is the number of predecessors of the Basic Block containing this Phi.
    // We do not store the length here to save space.
    uint32_t operand_start_index;
} phi_operand_t;


// A massive contiguous array storing all operands for all Phis.
uint32_t phi_operand_pool[???];
```
## Instruction Encoding

```text
63 62        57 56     54 53        48 47      42 41        28 27        14 13        00
|  |----------| |-------| |----------| |--------| |----------| |----------| |----------|
e       cs         wid        TODO        opc         def          src1         src2
```

### Encoding Symbols

<**src2**>  14-bit index for `ssa_versions[]`.

<**src1**>  14-bit index for `ssa_versions[]`.

<**def**>  14-bit index for `ssa_versions[]`.

<**opc**>  Instruction opcode.

<**TODO**> Still figuring out what to out here.

<**wid**> Instruction width type:

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

<**cs**> Since `instruction_t` only support 2 args, this is for instructions where the 3rd arg is a constant:

* Vector Element (`EXT`, `INS`, `DUP`)

    - Used for SIMD lane manipulation where the index must be an immediate.

    ```text
    62 61 60       57
    |---| |---------|
      r       imm
    ```

    - <**imm**> Vector lane index  (0-15).

    - <**r**> Reserved.

* Branch and Compare (`B.cond`, `CSET`, `CSEL`)

    ```text
    62 61 60       57
    |---| |---------|
      r       imm
    ```

| Encoding | Mnemonic | Meaning |
| :--- | :--- | :--- |
| `0000` | **EQ** | Equal |
| `0001` | **NE** | Not Equal |
| `0010` | **CS / HS** | Carry Set / Unsigned Higher or Same |
| `0011` | **CC / LO** | Carry Clear / Unsigned Lower |
| TODO | TODO | TODO | 
| `1110` | **AL** | Always |

<**e**> Set to 1 if `def`, `src1`, or `src2` is greater than 16383.

### Operational Information

If Flags.E is 1:

    * `instruction_t` is an index into `large_instructions[]`. We write the index into the 42-bit space.

```text
63 62        57 56     54 53        48 47      42 41                                  00
|  |----------| |-------| |----------| |--------| |------------------------------------|
e       cs         wid        TODO        opc                    index
```

## Instruction Design

```c
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
// instructions[20].source1= original_variables[5].reaching_definition;
// 
// 30: c1 = a1 + b1;
// instructions[30].source1 = original_variables[5].reaching_definition;

// ---------------------------------------------------------
// Instruction Bit Manipulation Macros
// ---------------------------------------------------------
// Layout: [Flags 16 | Op 6 | Def 14 | Src1 14 | Src2 14]

#define IR_MASK_14        0x3FFF
#define IR_MASK_6         0x3F
#define IR_MASK_16        0xFFFF

// Encoders
#define IR_ENCODE(flags, op, def, s1, s2) \   
    ( ((uint64_t)(flags) & IR_MASK_16) << 48 | \
      ((uint64_t)(op)    & IR_MASK_6)  << 42 | \
      ((uint64_t)(def)   & IR_MASK_14) << 28 | \
      ((uint64_t)(s1)    & IR_MASK_14) << 14 | \
      ((uint64_t)(s2)    & IR_MASK_14) )

// Decoders
#define IR_GET_FLAGS(i)   (((i) >> 48) & IR_MASK_16)
#define IR_GET_OP(i)      (((i) >> 42) & IR_MASK_6)
#define IR_GET_DEF(i)     (((i) >> 28) & IR_MASK_14)
#define IR_GET_SRC1(i)    (((i) >> 14) & IR_MASK_14)
#define IR_GET_SRC2(i)    ((i)         & IR_MASK_14)

typedef uint64_t instruction_t
instruction_t instructions[???];                 // High Density

typedef struct
{
    uint32_t definition;
    uint32_t source1;
    uint32_t source2;
    uint32_t _padding;
} large_instruction_t

large_instruction_t large_instructions[???];    // Low Density
size_t large_instructions_count;

```

## How to use `instruction_t` to index into `large_instructions[]`.
```c
// # Scenario
// 
// 1. `large_instructions[]` is empty.
// 2. Create a new instruction: `z1 = ADD v20000, v20000`.
// 3. `20000` is larger than `16383`. We must use the large pool.
//
// We see our counter `large_instructions_count = 0`. We write the actual values
// into the pool at index 0;
//
// ```
// large_instructions[0].definition = 1;
// large_instructions[0].source1 = 20000;
// large_instructions[0].source2 = 20000;
// ```
// 
// Finally we encode `instruction_t`.
//
// 1. Bits[0:41] = large_instructions_count;
// 2. Bits[47:42] = OPCODE_ADD
// 4. Bits[63] = 1 << 63 // Enable the Extended Flag
```
