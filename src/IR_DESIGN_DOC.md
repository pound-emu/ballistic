All memory will be allocated from a contiguous memory arena before the pass begins. No `malloc` calls inside the loop.

```c
// TODO: Design Dominator Tree, Basic Block, Instructions, Def-Use chains

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
// IT is the frontend's job to populate this array.
//
// Example:
//
// Instruction 100: x = y + z
//
// If `x` is at index 5, original_variables[5].reachingDef = 100.
// `x` maps to instruction 100.
original_variable_t original_variables[???];
```
