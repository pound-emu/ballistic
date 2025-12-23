All memory will be allocated from a contiguous memory arena before the pass begins. No `malloc` calls inside the loop.

```c
// TODO: Design Dominator Tree, Basic Block, Instructions
// Ref: Chapter 3.1.3
typedef struct
{
    type_t type; // TYPE_INT, TYPE_FLOAT

    // 1: x = 4        Defines x so reachingDef = 1 (x₁)
    // 2: y = x + 1    Only uses x so reachingDef is still 1.
    // 3: x = 5        Redefines x so reachingDef = 2 (x₂).
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
    int defining_instruction;
} ssa_version_t

// This array tracks the unique variables produced by the SSA (x1, x2, y1)
// 
// Example:
// 
// 1: x1 = 5 
// ssa_versions[x1].original_var_id = 1
// ssa_versions[x1].defining_instruction = 1
//
// 2: y1 = x1 + 5
// ssa_versions[y1].original_var_id = 2
// ssa_versions[y1].defining_instruction = 2
// 
// 3: x2 = 20
// ssa_versions[x2].original_var_id = 1
// ssa_versions[x2].defining_instruction = 3
ssa_version_t ssa_versions[???];
```
