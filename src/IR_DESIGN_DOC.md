All memory will be allocated from a contiguous memory arena before the pass begins. No `malloc` calls inside the loop.

# Table of Contents
1. [Flat SSA Model](#cfg)
2. [Structured SSA Model](#scfg)

# <a name="cfg" /> Flat SSA Model

This model uses a graph of Basic Blocks connected by jumps where dominance relationships must be calculated explicitly. It handles data flow merges by using ϕ-nodes.

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
    uint32_t original_variable_id;

    // If < TAG_CONSTANT: It's an index into the instructions array.
    // If >= TAG_CONSTANT: It's an index into the constants array (with the
    // high bit masked off).
    uint32_t defining_instruction;
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
} phi_node_t;

phi_node_t phi_nodes[???];

// A massive contiguous array storing all operands for all Phis.
uint32_t phi_operand_pool[???];
```
## Instruction Encoding

```text
63 62               53 52        39 38        25 24        11 10        00
|  |-----------------| |----------| |----------| |----------| |----------|
e          opc             def          src1         src2         src3
```

### Encoding Symbols

<**src3**>  11-bit index for `ssa_versions[]`.

<**src2**>  14-bit index for `ssa_versions[]`.

<**src1**>  14-bit index for `ssa_versions[]`.

<**def**>   14-bit index for `ssa_versions[]`.

<**opc**>   10-bit opcode.

<**e**>     Set to 1 if `def`, `src1`, or `src2` is greater than 16383.

### Operational Information

If Flags.E is 1:

    * `instruction_t` is an index into `large_instructions[]`. We write the index into the 53-bit space.

```text
63 62               53 52                                               00
|  |-----------------| |-------------------------------------------------|
e          opc                               index
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
// instructions[20].source1 = original_variables[5].reaching_definition;
// 
// 30: c1 = a1 + b1;
// instructions[30].source1 = original_variables[5].reaching_definition;

// ---------------------------------------------------------
// Instruction Bit Manipulation Macros
// ---------------------------------------------------------
// Layout: [Flag 1| Op 10 | Def 14 | Src1 14 | Src2 14 | Src3 11]

#define IR_MASK_14        0x3FFF
#define IR_MASK_11        0x7FF
#define IR_MASK_10        0x3FF
#define IR_MASK_1         0x1

// Encoders
#define IR_ENCODE(flag, op, def, s1, s2, s3) \   
    ( ((uint64_t)(flag)  & IR_MASK_1)  << 63  | \
      ((uint64_t)(op)    & IR_MASK_10) << 53 | \
      ((uint64_t)(def)   & IR_MASK_14) << 39 | \
      ((uint64_t)(s1)    & IR_MASK_14) << 25 | \
      ((uint64_t)(s2)    & IR_MASK_14) << 14 | \
      ((uint64_t)(s3)    & IR_MASK_11))

// Decoders
#define IR_GET_FLAG(i)    (((i) >> 63) & IR_MASK_1)
#define IR_GET_OP(i)      (((i) >> 53) & IR_MASK_10)
#define IR_GET_DEF(i)     (((i) >> 39) & IR_MASK_14)
#define IR_GET_SRC1(i)    (((i) >> 25) & IR_MASK_14)
#define IR_GET_SRC2(i)    (((i) >> 14) & IR_MASK_14)
#define IR_GET_SRC3(i)    ((i)         & IR_MASK_11)

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

## Terminal Design
```c
typedef enum 
{ 
    TERMINAL_FALLTHROUGH, // Fall through to the physically next block in memory
    TERMINAL_BRANCH,      // Unconditional jump
    TERMINAL_COND_BRANCH, // Conditional jump (if/else)
    TERMINAL_RET,         // Return from function
    TERMINAL_INDIRECT     // Branch to Register (Jump Table / Function Pointer)
} terminal_type_t;

typedef struct
{
    terminal_type_t type;
    union
    {
        // For TERMINAL_BRANCH and TERMINAL_COND_BRANCH
        //
        struct
        {
            uint32_t target_true_block_index;
            uint32_t target_false_block_index;
        } direct;

        // For TERMIMAL_INDIRECT
        // You cannot statically analyze the target of this.
        // It relies on a runtime value in a register.
        //
        struct 
        {
            uint32_t target_register_ssa_index; 
        } indirect;
    };
} terminal_t;

terminal_t terminals[???];
```

## Basic Block Design
```c
typedef struct
{
    uint32_t instruction_start_index;
    uint32_t phi_start_index

    uint16_t instruction_count;
    uint16_t phi_count;

    uint16_t successor_start_index;
    uint16_t successor_count;
    
    uint16_t predecessor_start_index;
    uint16_t predecessor_count;

    // If variable gets defined in Block X, this tracks where the definition
    // collide with other blocks.
    //
    // If-Else:
    //
    //      A
    //     / \
    //    B   C
    //     \ /
    //      D
    //
    // B defines x = 1.
    // B dominates itself. It does not dominate D.
    // The frontier of B is D.
    // We insert x = phi(...) in D.
    // basic_blocks[B].dominance_frontier_start = D
    // 
    // For Loop: 
    //
    //      A <---|   
    //      |     |
    //      B --->|
    //
    // B defines i = i + 1
    // B dominates itself. It does not dominante A.
    // The frontier of B is A.
    // We insert i = phi(...) in A.
    // badic_blocks[B].dominance_frontier_start = A;
    //
    uint16_t dominance_frontier_start;
    uint16_t dominance_frontier_count;

    // If this is the entry block, this index is invaild.
    //
    uint16_t immediate_dominator_index;
    uint16_t dominator_children_start_index;
    uint16_t dominator_children_count;

    uint32_t terminal_index;

    // Used to order hot and cold blocks in memory.
    uint32_t execution_count;
} basic_block_t;

// All lists (predecessors, successors, dominance frontiers) are slices of this array.
uint32_t block_indices[???];

basic_block_t basic_blocks[???];
```
# <a name="scfg" /> Structured SSA Model

This replicates Dynarmic's IR layer but it respects SSA by using Block Arguments. Branches push values into the target scope like function parameters.

```c
// Scenario
//
x = 0;
if (cond) { x = 10; } else { x = 20; }
print(x);


// Structured SSA Representation
//
v3 = IF (cond) TARGET_TYPE: Int64 // The IF instruction defines v3
{
    YIELD 10    // Pushes 10 to v3
}
ELSE
{
    YIELD 20    // Pushes 20 to v3
}

PRINT v3
```

## Variable Design

```c
typedef struct
{
    uint32_t current_ssa_index;
    uint32_t original_variable_index;
} source_variable_t;

sourve_variable_t source_variables[???];

typedef struct
{
    type_t type; // TYPE_INT, TYPE_FLOAT, etc.
    uint32_t defining_instruction_index;
    uint16_t use_count;
} ssa_version_t;

ssa_version_t ssa_versions[???];
```

## Instruction Encoding

```text
63               54 53        36 35        18 17        00
|-----------------| |----------| |----------| |----------|
        opc             src1         src2         src3
```

### Encoding Symbols

<**src3**>  18-bit index for `ssa_versions[]`.

<**src2**>  18-bit index for `ssa_versions[]`.

<**src1**>  18-bit index for `ssa_versions[]`.

<**opc**>   10-bit opcode.

## Instruction Design

```c
typedef uint64_t instruction_t;
instruction_t instructions[???];
uint32_t instruction_count;
```

## How do we know when a variable is created?

We use Implicit Indexing:

1. When we create an instruction at `instructions[100]`, we have implicitly created the variable definition at `ssa_versions[100]`.

2. We do not "check" if it is created. The act of incrementing `instruction_count` creates it.

This removes the need for an explicit `definition` bitfield in `instructions_t`, creating space to expand `src1`, `src2`, and `src3` to 18 bits.
