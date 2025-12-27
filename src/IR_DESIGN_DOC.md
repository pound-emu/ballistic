All memory will be allocated from a contiguous memory arena before the pass begins. No `malloc` calls inside the loop.

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
// This struct is *only* used for SSA construction. It maps the program's original state (like Guest Registers) to the current SSA variable. 
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

## Control Instructions Design

Control Instructions defines nested scopes (Basic Blocks). They produce SSA variables. These will replace phi-nodes and terminals.

1. `OPCODE_IF`
    * **Input**: Condition variable.
    * **Structure**: Creates "Then" and "Else" blocks.
    * **Output**: Defines SSA variables representing the result of the executed branch.

2. `OPCODE_LOOP`
    * **Input**: Initial loop arguments (optional).
    * **Structure**: Creates a "Body" block.
    * **Output**: Defines SSA variables representing the state when the loop terminates.

3. `OPCODE_BLOCK`
    * **Structure**: Creates a single nested scope.
    * **Output**: Defines SSA variables yielded by the block.

4. `OPCODE_YIELD`
    * **Role**: Data Flow.
    * **Behaviour**: Pushes a value from inside a child scope (Then/Else/Body) to the parent Control Instruction (IF/LOOP), resolving tbe SSA merge.

5. `OPCODE_BREAK`
    * **Role**: Control Flow.
    * **Behaviour**: Exits a `LOOP` or `BLOCK` scope immediately. Can carry values to the target scope.

6. `OPCODE_CONTINUE`
    * **Role**: Control Flow.
    * **Behaviour**: Jumps to the header of the nearest enclosing `LOOP` scope. Can carry values to update loop arguments.

7. `OPCODE_RETURN`
    * **Role**: Function Exitu
    * **Behaviour**: Not exactly sure about this one yet. How would function inlining work?

## How do we know when a variable is created?

We use **Implicit Indexing**:

1. When we create an instruction at `instructions[100]`, we have implicitly created the variable definition at `ssa_versions[100]`.

2. We do not "check" if it is created. The act of incrementing `instruction_count` creates it.

This removes the need for an explicit `definition` bitfield in `instructions_t`, creating space to expand `src1`, `src2`, and `src3` to 18 bits.

## What about instructions that don't define anything?

If `instructions[200]` is `STORE v1, [v2]`, it defines no variable for other instructions to use. If we strictly follow implicit indexing, we create `ssa_values[200]`. Is this waste?

**Yes. Its 8 bytes of waste. But this is the best choice we have.**

Its either this or keep track of a `definition` field in `instruction_t` which then shrinks `src1`, `src2`, and `src3` to a tiny 14 bits.

We handle these void instructions by marking the SSA variable as `VOID`: `ssa_versions[200].type = TYPE_VOID`

### What about control instructions that defines more than one variable?

We use **Proxy Instructions** to handle multiple definitions while keeping our O(1) Array Indexing: `OPCODE_PROXY`

If an `IF` statement needs to define 2 variables (x, y), we create 3 sequential instructions.

1. The primary `IF` instruction (defines x).
2. A proxy instruction immediately following it (defines y).

These proxy instructions should generate **ZERO** machine code.

#### Scenario
```c
int x;
int y;
if (condition) {
    x = 10; y = 20;
} else {
    x = 30; y = 40;
}
// x and y are used here.
```

#### Structured SSA Representation

```text
// Define the first merge value (x -> v100).
//
v100 = OPCODE_IF (v_cond)  TARGET_TYPE: INT64

// This proxy defines the second merge value (y -> v101)
// It explicity references the parent IF(v100) to attach itself..
//
v101 = OPCODE_PROXY (v100) TARGET_TYPE: INT64
{
    v102 = OPCODE_CONST 10
    v103 = OPCODE_CONST 20

    // v102 -> v100 (x)
    // v103 -> v101 (y)
    //
    OPCODE_YIELD v102, v103
}
ELSE
{
    v104 = OPCODE_CONST 30
    v105 = OPCODE_CONST 40

    // v104 -> v100 (x)
    // v105 -> 101 (y)
    //
    OPCODE_YIELD v104, v105
}

OPCODE_PRINT v100
OPCODE_PRINT v101

```

#### Memory Layout in `instructions[]`
```text
| Index | Opcode                  | Operands             | SSA Value Created |
|-------|-------------------------|----------------------|-------------------|
| 100   | OPCODE_IF               | src1: v_cond         | v100 (defines x)  |
| 101   | OPCODE_PROXY_DEFINITION | src1: 100 (Parent)   | v101 (defines y)  |
| 103   | OPCODE_MOVZ (x = 10)    | src1: v100, src2: 10 | v103              |
| 104   | OPCODE_MOVZ (y = 20)    | src1: v101, src2: 20 | v104              |
```
