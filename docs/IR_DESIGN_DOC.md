# Structured SSA Model

This replicates [Dynarmic's](https://github.com/pound-emu/dynarmic) IR layer 
but it respects SSA by using Block Arguments. Branches push values into the
target scope like function parameters.

# Data Structures

## Variables

```c
// This struct is *only* used for SSA construction. It maps the program'
// original state (like Guest Registers) to the current SSA variable. 
typedef struct
{
    uint32_t current_ssa_index;
    uint32_t original_variable_index;
} source_variable_t;

sourve_variable_t source_variables[???];

typedef struct
{
    uint8_t  type; // INT32, FLOAT, etc.
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
63               54 53        36 35        18 17        00
|-----------------| |----------| |----------| |----------|
        opc             src1         src2         src3
```

### Encoding Symbols

<**src3**>  18-bit index for `ssa_versions[]`.

<**src2**>  18-bit index for `ssa_versions[]`.

<**src1**>  18-bit index for `ssa_versions[]`.

<**opc**>   10-bit opcode.

### Operational Information

If Bit[17] in `src1`, `src2`, or `src` is 1, the operand is a index into `constant_pool[]`.  It has no SSA index. It has no entry in `ssa_versions`.

## Instructions

```c
typedef uint64_t instruction_t;
instruction_t instructions[???];
uint32_t instruction_count;
```

# Instruction Set Architecture

## Control Instructions

Control Instructions sefines nested scopes (Basic Blocks). They produce SSA
variables. These will replace phi-nodes and terminals.

1. `OPCODE_IF`
    * **Input**: Condition variable.
    * **Structure**: Creates "Then" and "Else" blocks.
    * **Output**: Defines SSA variables representing the result of the executed
      branch.

2. `OPCODE_LOOP`
    * **Input**: Initial loop arguments (optional).
    * **Structure**: Creates a "Body" block.
    * **Output**: Defines SSA variables representing the state when the loop
      terminates.

3. `OPCODE_BLOCK`
    * **Structure**: Creates a single nested scope.
    * **Output**: Defines SSA variables yielded by the block.

4. `OPCODE_YIELD`
    * **Role**: Data Flow.
    * **Behaviour**: Pushes a value from inside a child scope (Then/Else/Body)
      to the parent Control Instruction (IF/LOOP), resolving tbe SSA merge.

5. `OPCODE_BREAK`
    * **Role**: Control Flow.
    * **Behaviour**: Exits a `LOOP` or `BLOCK` scope immediately. Can carry
      values to the target scope.

6. `OPCODE_CONTINUE`
    * **Role**: Control Flow.
    * **Behaviour**: Jumps to the header of the nearest enclosing `LOOP` scope.
      Can carry values to update loop arguments.

7. `OPCODE_RETURN`
    * **Role**: Function Exit
    * **Behaviour**: Not exactly sure about this one yet. How would function
      inlining work?

## Extension Instructions

If an operation requires more than 3 operands (like `YIELD` returning 5 values),
we insert instruction **immediately** preceding the consumer to carry the extra
load.

### Opcode Design

1. `OPCODE_ARG_EXTENSION`
    * **Role**: Holds 3 operands that are pushed to the next instruction.
    * **Output**: `TYPE_VOID

2. `OPCODE_DEF_EXTENSION`
    * **Role**: Extends the definitiom list of the preceding Control Instruction
      to support multiple merge values.
    * **Output**: Defines a valid SSA variable representing the next value in
      the merge set.

### Scenario 1: `OPCODE_YIELD v1, v2, v3, v4, v5`,

We cannot fit 5 operands into one `instruction_t`. We split them.

Memory Layout in `instructions[]`

| Index | Opcode               | src1 | src2 | src3 | SSA Def | Comment                    |
|-------|----------------------|------|------|------|---------|----------------------------|
| 100   | OPCODE_ARG_EXTENSION | v4   | v5   | NULL | v100    | Carries args 4 and 5       |
| 101   | OPCODE_YIELD         | v1   | v2   | v3   | v101    | Carries args 1-3 & Executes|


### Scenario 2: `x1, y1 = OPCODE_IF (vcondition) TARGET_TYPE: INT`

This `IF` block defines 2 variables. Since this IR is designed to define one
variable per instruction, we split `x1`, and `y1` into two seperate instructions.

Memory Layout in `instructions[]`

| Index | Opcode               | src1 | src2 | src3 | SSA Def | Comment         |
|-------|----------------------|------|------|------|---------|-----------------|
| 100   | OPCODE_IF            |vcond | NULL | NULL | v100    | Definition of x |
| 101   | OPCODE_DEF_EXTENSION | v100 | NULL | NULL | v101    | Definition of y |

# SSA Construction Rules

## 1. Control Flow Rules

### Rule 1.1: Yielding

In an `IF/ELSE` structure, the `THEN` block and the `ELSE` block must yield
the exact same number of values with the exact same types.

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

### Rule 4.2: Hot-Cold Splitting

If a basic block is deemed cold, it should move to a separate buffer.

### Rule 4.3: Source Variables

Throw away `source_variables[]` after SSA construction. Do not keep it in
memory.

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
v2 = OPCODE_IF (v_cond) TARGET_TYPE: INT

    // i++
    //
    v3 = OPCODE_ADD v0, 1

    OPCODE_YIELD v3

OPCODE_END_BLOCK


// We then see that `i` changed from v0 to v2.
// Now we create the loop with arg v2.
// v4 is the Phi for `i`.
//
v4 = OPCODE_LOOP (v2) TARGET_TYPE: INT
    // Loop condition check.
    //
    v5 = OPCODE_CMP_LT v4, v1
    
    v6 = IF (v5) TARGET_TYPE: VOID

        // Cloned body.
        // Changed operand v0 to v4.
        //
        v6 = OPCODE_ADD v4, 1

        OPCODE_CONTINUE v6
    
    OPCODE_ELSE
        
        OPCODE_BREAK v5

    OPCODE_EMD_BLOCK

OPCODE_EMD_BLOCK
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

# Tiered Compilation Strategy

## Tier 1: Dumb Translation

* Greedy Register Allocator.
* Pre-defined machine code templates for code generation.
* No optimizations **except** for Peepholes. To make peepholing as fast as
  possible, we use a sliding window while emitting the machine code.

We switch to tier 2 when a basic block turns hot.

## Tier 2: Optimized Translation

* Run all required optimizations passes.

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

// v4 Represents the loop expression.
// Inside the block, v4 is the phi-node for `sum`.
// Outside the block, v4 is the final result returned by `OPCODE_BREAK`.
//
v4 = OPCODE_LOOP (v2, v3) TARGET_TYPE: INT64

// v6 is the phi-node for `i`.
// It attaches to v4 to handle the second loop argument.
//
v5 = OPCODE_DEF_EXTENSION (v4) TARGET_TYPE: INT64
    // ---------------------------------------------------------
    // 3. LOOP BODY
    // ---------------------------------------------------------

    i < limit
    v6 = OPCODE_CMP_LT v5, v1

    // Decide to continue or exit.
    // v7 represents the result of this IF block.
    //
    v7 = OPCODE_IF (v6) TARGET_TYPE: VOID
    
        // val = array[i]
        //
        v8 = OPCODE_LOAD v0, v5

        // val > 0
        //
        v9 = OPCODE_CONST 0
        v10 = OPCODE_CMP_GP v8, v9

        // v11 is the new sum after `v > 0`.
        //
        v11 = OPCODE_IF (v10) TARGET_TYPE: INT64
        
            // sum += val
            //
            v12 = OPCODE_ADD v4, v8

            // v12 -> v11
            //
            OPCODE_YIELD v12
        
        OPCODE_ELSE

            // v4 -> v11
            //
            OPCODE_YIELD v4

        OPCODE_END_BLOCK // Terminates the entire IF structure.

        // ++i (v5)
        //
        v13 = OCPODE_CONST 1
        v14 = OPCODE_ADD v5, v13

        // Jump to loop header.
        // v11 (New Sum) -> v4 (Phi Sum)
        // v14 (New i)   -> v5 (Phi i)
        //
        OPCODE_CONTINUE v11, v14
    
    OPCODE_ELSE

        // Exit loop.
        //
        OPCODE_BREAK v4

    OPCODE_END_BLOCK // Terminates the entire IF structure.

OPCODE_END_BLOCK // Terminantes the entire loop.


// ---------------------------------------------------------
// 4. EXIT
// ---------------------------------------------------------

// v4 holds the final sum.
OPCODE_RETURN v4
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

