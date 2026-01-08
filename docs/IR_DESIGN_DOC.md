# Structured SSA Model

This replicates [Dynarmic's](https://github.com/pound-emu/dynarmic) IR layer 
but it respects SSA by using Block Arguments. Branches push values into the
target scope like function parameters.

# Data Structures

## Variables

```c
// This struct is *only* used for SSA construction. It maps the program'
// original state (like Guest Registers) to the current SSA variable. 
//
typedef struct
{
    uint32_t current_ssa_index;
    uint32_t original_variable_index;
} source_variable_t;

source_variable_t source_variables[???];

typedef struct
{
    uint8_t  bit_width; // 8/16/32/64.
    uint16_t use_count;
} ssa_version_t;

ssa_version_t ssa_versions[???];
```

## Constants

```c
uint32_t constant_pool[???];
```

## Instruction Encoding

```text
63               53 52        35 34        17 16        00
|-----------------| |----------| |----------| |----------|
        opc             src1         src2         src3
```

### Encoding Symbols

<**src3**>  17-bit index for `ssa_versions[]`.

<**src2**>  17-bit index for `ssa_versions[]`.

<**src1**>  17-bit index for `ssa_versions[]`.

<**opc**>   11-bit opcode.

### Operational Information

If Bit[16] in `src1`, `src2`, or `src` is 1, the operand is a index into `constant_pool[]`.  It has no SSA index. It has no entry in `ssa_versions`.

## Instructions

```c
typedef uint64_t instruction_t;
instruction_t instructions[???];
uint32_t instruction_count;
```

## Block Scope

This was created to find out how many variables will be modified in
`IF/LOOP` blocks.

```c
typedef struct
{
    uint32_t type;          // BLOCK_TYPE_IF, BLOCK_TYPE_LOOP
    uint32_t start_index;   
    int32_t  yield_arity;   // How many SSA variables to create when merging.
                            // -1 = Unknown (First branch).
} block_scope_t;

// Keep track of nested blocks.
//
block_scope_t block_scope_stack[64];
int stack_depth = 0;
```

### Scenario

```text
// Some instructions have been ommited to keep this example simple.
//
IF (A)
    IF(B)
        YIELD w
    ELSE
        YIELD x
    y = MERGE   // End Inner IF Block.
    YIELD y
ELSE
    YIELD z
a = MERGE      // End Outer IF Block.
```

### Memory Representation in `block_scope_stack[]`

```text
// IF/ELSE combined is 1 block.
// 100: block_scope_stack[0] = {.type = IF, .start =  100, .yield_arity = -1}
// stack_depth = 1
//
IF (A)

    // 101: block_scope_stack[1] = {.type = IF, .start = 101, .yield_arity = -1}
    // stack_depth = 2
    //
    IF(B)
        
        // We peek at the top of stack.
        //
        // block_scope_stack[1].yield_arity == -1
        //
        // Therefore, we set our current block's arity to one.
        //
        // 102: block_scope_stack[1].yield_arity = 1 
        // 
        YIELD w

    // Stack does not change here.
    //
    ELSE

        // We peek at the top of stack.
        //
        // block_scope_stack[1].yield_arity == 1
        //
        // Does this current yield have 1 value? Yes. So we do not modify the
        // block stack.
        //
        YIELD x

    // We pop the stack, and retrive `yield_arity = 1`.
    // This tells us to define ONE variable and merge w and x into y.
    //
    // This does NOT affect block_scope_stack[0].
    //
    y = MERGE

    // We peek at the top of stack.
    //
    // block_scope_stack[0].yield_arity == -1
    //
    // Since this is the first yield of the outer block, we set the arity now.
    //
    // block_scope_stack[0].yield_arity = 1
    //
    YIELD y

// Stack does not change here.
ELSE

    // We peek at the top of stack.
    //
    // block_scope_stack[0].yield_arity == 1
    //
    // Does this current yield arity match 1? Yes. We leave the stack alone.
    //
    YIELD z

// We pop the stack. We see `yield_arity == 1`, so only one variable gets
// defined. We merge y and z into a.
//
a = MERGE     // End Outer IF Block.
```

# Instruction Set Architecture

## Control Instructions

Control Instructions sefines nested scopes (Basic Blocks). They produce SSA
variables. These will replace phi-nodes and terminals.

1. `OPCODE_IF`
    * **Input**: Condition variable.
    * **Structure**: Creates "Then" and "Else" blocks.
    * **Output**: None.

2. `OPCODE_LOOP`
    * **Input**: Initial loop arguments (optional).
    * **Structure**: Creates a "Body" block.
    * **Output**: Defines SSA variables representing the state when the loop
      terminates. 

3. `OPCODE_BLOCK_ARG`
    * **Input**: Immediate Index.
    * **Output**: Defines a valid SSA variable representing  the *Nth* argument
      passed to the block.
    * This must be the first instruction(s) inside a `LOOP` body.
    * On the first iteration, it takes the value from `OPCODE_LOOP` inputs.
      On subsequent iterations, it takes the value from `OPCODE_CONTINUE`
      inputs.

4. `OPCODE_MERGE` 
    * **Input**: None. Implicitly pops values from `block_scope_stack[]`.
    * **Output**: Defines the merged SSA variable.

5. `OPCODE_YIELD`
    * **Role**: Data Flow.
    * **Behaviour**: Pushes a value from inside a child scope (Then/Else/Body)
      to the parent Control Instruction (IF/LOOP), resolving tbe SSA merge.

6. `OPCODE_BREAK`
    * **Role**: Control Flow.
    * **Behaviour**: Exits a `LOOP` or `BLOCK` scope immediately. Can carry
      values to the target scope.

7. `OPCODE_CONTINUE`
    * **Role**: Control Flow.
    * **Behaviour**: Jumps to the header of the nearest enclosing `LOOP` scope.
      Can carry values to update loop arguments.

8. `OPCODE_RETURN`
    * **Role**: Function Exit
    * **Behaviour**: Not exactly sure about this one yet. How would function
      inlining work?

## Extension Instructions

### Opcode Design

* `OPCODE_ARG_EXTENSION`
    * **Role**: Holds 3 operands that are pushed to the next instruction.
    * **Output**: `TYPE_VOID

If an operation requires more than 3 operands (like `YIELD` returning 5 values),
we insert instruction **immediately** preceding the consumer to carry the extra
load.

### Scenario: `OPCODE_YIELD v1, v2, v3, v4, v5`

We cannot fit 5 operands into one `instruction_t`. We split them.

Memory Layout in `instructions[]`

| Index | Opcode               | src1 | src2 | src3 | SSA Def | Comment                    |
|-------|----------------------|------|------|------|---------|----------------------------|
| 100   | OPCODE_ARG_EXTENSION | v4   | v5   | NULL | v100    | Carries args 4 and 5       |
| 101   | OPCODE_YIELD         | v1   | v2   | v3   | v101    | Carries args 1-3 & Executes|

# SSA Construction Rules

## 1. Control Flow Rules

### Rule 1.1: Symmetric Yielding

In an `IF/ELSE` structure, the `THEN` block and the `ELSE` block must yield
the exact same number of values with the exact same types.

If the source code has an `IF` without an `ELSE`, Ballistic must generate an
`ELSE` block. For every variable yielded in the `THEN` block, the `ELSE` block
must be yield the variable before the `IF` started.

### Rule 1.2: Loop Arguments

`OPCODE_LOOP` must define its phi-node (loop arguments) immediately.
`OPCODE_CONTINUE` at the bottom of the loop must match the arity and types
of the `LOOP` header exactly. You cannot add a loop argument *later* during
the pass. If you need a phi-node, you must rebuild the loop header.

### Rule 1.3: `OPCODE_IF` and `OPCODE_CONDITIONAL_SELECT`

SSA construction will emit `OPCODE_IF` by default for branches. If the block is
very small we flatten it to `OPCODE_CONDITIONAL_SELECT` via the
[If-to-Select Optimization Pass](#if-to-select-optimization-pass).

In general we use `OPCODE_IF` for side-effect heavy operations, and use
`OPCODE_CONDITIONAL_SELECT` for safe arithmatic.

### Rule 1.4: Loop Positional Mapping

Loop arguments map strictly 1-to-1 based on their position in the instruction
stream.

1. **Definitions**: `OPCODE_LOOP` defines the 1st Phi variable. The following
   `OPCODE_DEF_EXTENSION instruction defines the 2nd, 3rd... Nth Phi variables.

2. **Arguments**: If the loop or if block require more than 3 initial values, 
   `OPCODE_ARG_EXTENSION` must be inserted immediately before `OPCODE_LOOP` or
   `OPCODE_IF`.

3. **Continue**: The operands passed to `OPCODE_CONTINUE` update the  1st,
   2nd, 3rd... Nth Phi variables for next iteration.

Example: If `LOOP_OPCODE` defines `[v1, v2]`, and `OPCODE_CONTINUE` passes 
`[v8, v9]`, then `v8` flows into `v1`, and v9` flows into `v2`.

### Rule 1.5: Block Termination

When closing a scope, Ballistic must determine whether to emit `OPCODE_MERGE`
or `OPCODE_END_BLOCK` based on reachability.

* If **ANY** path (Then or Else) does not have `OPCODE_BREAK`,
  `OPCODE_CONTINUE` or `OPCODE_RETURN`, you must emit `OPCODE_MERGE`.

* IF **ALL** paths contain `OPCODE_BREAK`, `OPCODE_CONTINUE`, or
  `OPCODE_RETURN`, all paths are unreachable. Emit `OPCODE_END_BLOCK`
  only. 

## Data Flow Rules

### Rule 2.1: Single Static Assignment

Every instruction defines exactly one SSA ID (or Void). Instructions with
multiple return values and instruction arguments are handled via
[extension instructions](#extension-instructions).

### Rule 2.2: Contiguous Extension Instructions

Extention Instructions must be physically contiguous to their parent
instruction. No other instruction can exist between them.

### Rule 2.3: Constants

Constants are loaded via pool indices. We do not store raw literals in
operands.

### Rule 2.4: Immutable SSA

Once an instruction is written and the `instruction_count` increments, that SSA
definition is immutable. You cannot change the opcode of v100 from `ADD` to 
`SUB`.

If the logic needs to be changed during optomization, you must append a new
instruction and update and update the users to point to the new index.

You can **only** change an instruction to `OPCODE_NOP` to kill it.

## Type System Rules

### Rule 3.1: Typed Definitions

The type of a variable is defined only in `ssa_versions[]`. Instructions that
**use** variables do not encode the expected type.

This reduces opcode density. `OPCODE_ADD` doesnt need to say `OPCODE_ADD_INT32`.
It just says `OPCODE_ADD v1, v2`. Ballistic will look up `v1`'s type. If `v1`
and is a float and `v2` is int, Ballistic throws an error.

### Rule 3.2: Void Type

Any instructions that do not produce a value must define a variable with
`TYPE_VOID`. No other instruction can reference this variable as a source
operand.

## 4. Memory Bandwidth Rules

All memory will be allocated from a contiguous memory arena before the pass 
begins. No `malloc` calls during construction.

### Rule 4.1: Deleted Instructions

Since we are using implicit indexing, we cannot easily delete instructions.
Therefore we replace dead code with `NOP`. If the ratio of `NOP`'s to 
`instructions` exceed a threshold (like 25%), we must trigger a Compaction Pass.

The Compaction Pass will create a new `instructions[]`, remmaping all SSA
indices to be contiguous again, and discards the old array.

### Rule 4.2: Source Variables

Throw away `source_variables[]` after SSA construction. Do not keep it in
memory.

### Rule 4.3: Block Size Limit

The IR has a hard limit of 131,072 instructions due to thr 18-bit operand
encoding.

1. Checks must ensure `instruction_count` does not exceed 130,000. We leave a
   safety margin for final mergers/exits.
2. If the limit is reached, the compiler must terminate the block early, even
   if the guest function has not ended.
3. Let the runtime handle the next chunk as a separate compilation unit.

# Algorithms

## Loop Construction

We use Iterative Peeling to construct loops with two iterations. The first
iteration is a **Probe** to detect state change, then we clone the body into
a loop.

### Step 1: Peeling

We treat the first iteration as conditional branch.

1. Record the mapping of guest registers to current SSA variables as a snapshot.
2. Emit `OPCODE_IF`.
3. Emit the body.
4. When we encounter a back edge, we compare the current register map against
   the snapshot. Any variable *V* where `register_map[V] != snapshot_map[V]`
   is a Loop Argument.

### Step 2: Backpatching

Now we know the arity of the loop. We construct the loop header immediately
after the `IF`.

1. Emit `OPCODE_END_BLOCK` for the `IF`.
2. Emit `OPCODE_LOOP`.
    * **Inputs**: The SSA IDs yielded by the `IF`.
    * **Output: Define the new SSA phi-nodes for the loop arguments.

### Step 3: Cloning the Body

Populate the loop body by cloning the `IF` instructions.

1. Iterate linearly through the `IF`'s instruction range.
2. Remap the instruction operands.
    * If the operand is defined before `IF`, keep the original index.
    * If the operand is defined inside `IF`, we remap it to insidr the loop.
    * If the operand was an input to `IF`, replace it with the new loop phi
      node.

### Scenario

`int i = 0; while(i < 10) i++;`

### Structured SSA Representation

```text
// int i = 0
// 
v0 = OPCODE_CONST 0

// limit
//
v1 = OPCODE_CONST 10

// We don't know `i` changes yet. Just emit an IF
// v_cond = i < 10
//
OPCODE_IF (v_cond)

    // i++
    //
    v3 = OPCODE_ADD v0, 1

    OPCODE_YIELD v3

OPCODE_ELSE
    
    OPCODE_YIELD v0

// Merge the IF exit values (v3 and v0 into v4).
//
v4 = OPCODE_MERGE


// We then see that `i` changed from v0 to v4.
// Now we create the loop with arg v4.
//
OPCODE_LOOP (v4)

    // The 0th argument of this block (v4).
    //
    v5 = OPCODE_BLOCK_ARG 0

    // Loop condition check.
    //
    v6 = OPCODE_CMP_LT v5, v1
    
    IF (v6)

        // Cloned body.
        // Changed operand v0 to v5.
        //
        v7 = OPCODE_ADD v5, 1

        OPCODE_CONTINUE v7
    
    OPCODE_ELSE
        
        OPCODE_BREAK v5

    OPCODE_END_BLOCK

OPCODE_END_BLOCK

// Merge the LOOP exit values (v5 into v8).
//
v8 = OPCODE_MERGE
```

## IF-to-SELECT Optimization Pass

We look for Diamond Patterns in `IF` block control flows that are small enough
to be flattened into `OPCODE_CONDITIONAL_SELECT`

If the body contains no side effects and the instruction count < 2, we flatten
it. This removes branches which can be mispredicted.

```text
v0 = ...
v1 = ...
v_cond = ...

// Speculatively execute both paths
//
v_true = OPCODE_ADD v0, v1
v_false = OPCODE_SUB v0, v1

// Select the correct result
//
v_result = OPCODE_SELECT v_cond, v_true, v_false
```

## Emitting Yields In `ELSE` Blocks

Whe you reach the end of the `THEN` block, you have a list of dirty variables
(variables that changed and got yielded).

When generating an `ELSE` block, you must iterate through the dirty list.

1. Did the `THEN` block yield a new variable for register `R`.
2. If yes, does the `ELSE` block have a new value for `R`?
3. If no, emit `OPCODE_YIELD R` where `R` is the register before entering the
   `IF` block.

# Tiered Compilation Strategy

## Tier 1: Dumb Translation

* Greedy Register Allocator.
* Pre-defined machine code templates for code generation.
* No optimizations **except** for Peepholes. To make peepholing as fast as
  possible, we use a sliding window while emitting the machine code.

We switch to tier 2 when a basic block turns hot.

## Tier 2: Optimized Translation

* Run all required optimizations passes.
* During code generation, if a basic block is deemed cold, it should move to a
  separate buffer in a memory region far away.


## Required Optimization Passes

1. Register Allocation
2. Constant Folding & Propagation
3. Dead Code Elimination
4. Peepholes

# Proof of Concept

## Scenario

```c
// Calculate sum of array[i] where array[i] > 0
int64_t sum = 0;
int64_t i = 0;
while (i < limit) {
    int64_t val = array[i];
    if (val > 0) {
        sum += val;
    }
    i++;
}
return sum;
```

## Structured SSA Representation

```text
// ---------------------------------------------------------
// 1. ENTRY BLOCK
// ---------------------------------------------------------

// Array base address.
//
v0 = OPCODE_CONST 0x1000

// Loop limit.
//
v1 = OPCODE_CONST 100

// Sum definition.
//
v2 = OPCODE_CONST 0

// Iterator definition
//
v3 = OPCODE_CONST 0

// ---------------------------------------------------------
// 2. LOOP HEADER
// ---------------------------------------------------------

OPCODE_LOOP (v2, v3)

    // Represents v2 in this loop.
    //
    v4 = OPCODE_BLOCK_ARG 0

    // Represents v3 in this loop.
    //
    v5 = OPCODE_BLOCK_ARG 1

    // ---------------------------------------------------------
    // 3. LOOP BODY
    // ---------------------------------------------------------

    // i < limit
    //
    v6 = OPCODE_CMP_LT v5, v1

    // Decide to continue or exit.
    //
    OPCODE_IF (v6)
    
        // val = array[i]
        //
        v7 = OPCODE_LOAD v0, v5

        // val > 0
        //
        v8 = OPCODE_CONST 0
        v9 = OPCODE_CMP_GT v7, v8

        OPCODE_IF (v9)
        
            // sum += val
            //
            v10 = OPCODE_ADD v4, v7

            // v10 -> v11
            //
            OPCODE_YIELD v10
        
        OPCODE_ELSE

            // v4 -> v11
            //
            OPCODE_YIELD v4

        // Merges v10 and v4 into v11
        //
        v11 = OPCODE_MERGE

        // ++i (v5)
        //
        v12 = OCPODE_CONST 1
        v13 = OPCODE_ADD v5, v12

        // Jump to loop header.
        // v12 (New Sum) -> v4 (Phi Sum)
        // v13 (New i)   -> v5 (Phi i)
        //
        OPCODE_CONTINUE v12, v13
    
    OPCODE_ELSE

        // Exit loop.
        //
        OPCODE_BREAK v4

    OPCODE_END_BLOCK // Terminates the entire IF structure.

OPCODE_END_BLOCK // Terminantes the entire loop.

// The merge is required ti catch the BREAK value.
// Merges v4 into v14.
//
v14 = OPCODE_MERGE

// ---------------------------------------------------------
// 4. EXIT
// ---------------------------------------------------------

// v15 holds the final sum.
OPCODE_RETURN v14
```

# Frequently Asked Questions

## How do we know when a variable is created?

We use **Implicit Indexing**:

1. When we create an instruction at `instructions[100]`, we have implicitly
   created the variable definition at `ssa_versions[100]`.
2. We do not "check" if it is created. The act of incrementing
   `instruction_count` creates it.

This removes the need for an explicit `definition` bitfield in
`instructions_t`, creating space to expand `src1`, `src2`, and `src3` to 18
bits.

## What about instructions that don't define anything?

If `instructions[200]` is `STORE v1, [v2]`, it defines no variable for other
instructions to use. If we strictly follow implicit indexing, we create
`ssa_values[200]`. We handle these void instructions by marking the SSA
variable as `VOID`: `ssa_versions[200].type = TYPE_VOID`.

