All memory will be allocated from a contiguous memory arena before the pass begins. No `malloc` calls inside the loop.

```c
// TODO: Design Dominator Tree, Basic Block, Instructions, Def-Use chains

// Ref: Chapter 3.1.3
typedef struct
{
    instruction_t* defining_instruction;
    int reaching_definition;
} variable_t;
```
