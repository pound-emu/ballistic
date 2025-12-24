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

An open addressing hash map will need to be created to map constants to their index for `constants[]`

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
constant_t* constants[???];
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

    // The Topology (Intrusive Linked List)
    uint32_t next;
    uint32_t previous;

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
