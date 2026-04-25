# WASM Backend Design

> **Status:** Draft 2, addressing review feedback from [GloriousTacoo](https://github.com/GloriousTacoo)
>
> **Scope of this draft:** Tier 1 dumb emitter only. Covers the opcodes
> currently implemented and tested in the IR layer. Branching, control flow,
> and tiered optimization deferred per direction from [GloriousTacoo](https://github.com/GloriousTacoo)
>
> **Prerequisite reading:**
> - [`./IR_DESIGN_DOC.md`](./IR_DESIGN_DOC.md) - IR structure, SSA rules, scope rules
> - [`./PROGRAMMING_RULES.md`](./PROGRAMMING_RULES.md) - D-cache performance constraints

---

## Table of Contents

* [1. Overview](#1-overview)
* [2. Entry Point](#2-entry-point)
* [3. The IR Block Input](#3-the-ir-block-input)
* [4. File Structure](#4-file-structure)
* [5. Backend Context](#5-backend-context)
* [6. Operand Encoding Rule](#6-operand-encoding-rule)
* [7. Register File in Linear Memory](#7-register-file-in-linear-memory)
* [8. SSA → WASM Local Mapping](#8-ssa--wasm-local-mapping)
* [9. Locals Count Patching](#9-locals-count-patching)
* [10. WASM Module Structure](#10-wasm-module-structure)
* [11. Assembler Layer](#11-assembler-layer)
* [12. Backend Mapping Layer](#12-backend-mapping-layer)
* [13. Instruction Mapping Table](#13-instruction-mapping-table)
* [14. Out of Scope](#14-out-of-scope)
* [15. Planned: Copy Elision](#15-planned-copy-elision)
* [16. Open Questions](#16-open-questions)
* [17. Limitations](#17-limitations)

---

## 1. Overview

The WebAssembly (WASM) backend translates a completed Ballistic IR block into a WASM module
binary. Output is a flat byte buffer containing a valid WASM module with one
exported function `entry`. The buffer is handed to the platform layer, which
calls `WebAssembly.compile()` on the JS side.

The backend is intentionally dumb. It trusts the IR completely. It performs
no analysis, no optimization, and no multi-pass scanning. It maps IR opcodes
to WASM bytecode in a single linear pass over the instruction array.

The backend is selected at compile time. There is no runtime dispatch and no
function pointer overhead inside the translation loop.

```
cmake -DBALLISTIC_BACKEND=wasm ..   # web and other sandboxed targets
cmake -DBALLISTIC_BACKEND=x86  ..   # desktop/native
```

---

## 2. Entry Point

The engine calls one function after IR construction is complete:

```c
/**
 * Translate a completed IR block to a WASM module.
 *
 * On return, ctx->buf contains a valid WASM module ready for
 * WebAssembly.compile(). The platform layer hands the bytes off
 * to the host via the platform callback.
 */
void bal_backend_compile_block(BalBackendCtx* ctx, const bal_ir_block_t* block);
```

This is the only function the engine calls into the backend. The backend
owns its own iteration loop over the instruction array.

---

## 3. The IR Block Input

The backend consumes a `bal_ir_block_t` produced by the IR layer. The
backend reads only the fields listed below; everything else on the struct is
ignored. Full semantics for these fields live in
[`IR_DESIGN_DOC.md`](./IR_DESIGN_DOC.md).

```c
typedef struct {
    const bal_instruction_t* instructions;     /* flat array, walked by backend */
    uint32_t                 instruction_count;/* element count of instructions */
    const uint64_t*          constant_pool;    /* constants referenced by imm operands */
    /* … other fields owned by the IR layer … */
} bal_ir_block_t;
```

The backend treats `block` as strictly read-only. The `const` qualifier on
the function signature in §2 makes this enforceable.

---

## 4. File Structure

```
backend/
  wasm/
    assembler.h   - wasm_emit_* functions, all static inline
    assembler.c   - LEB128 encoding, buffer growth (out-of-line, branchy)
    backend.h     - bal_backend_compile_block() declaration
    backend.c     - IR opcode -> assembler call mapping, iteration loop
    module.h      - WASM module header/footer helpers
    module.c
```

Three layers, strictly separated:

- **assembler** - Has no knowledge of the IR
- **backend** - Has no knowledge of WASM binary encoding
- **module** - Has no knowledge of IR semantics

Each layer is independently replaceable.

---

## 5. Backend Context

```c
#define WASM_BUF_INITIAL_CAP 4096

typedef struct {
    uint8_t* buf;
    size_t   buf_len;
    size_t   buf_cap;

    uint32_t local_count;          /* count of WASM locals declared in body */
    size_t   locals_patch_offset;  /* byte offset of the locals-count slot  */

    uint64_t        guest_address; /* for cache key on the host             */
    BalWasmCallback callback;      /* called once with finished module      */
    void*           callback_userdata;
} BalBackendCtx;
```

`local_count` is the count of WASM locals the function will declare. It
increments only when the backend emits a value-producing IR instruction
(one that produces an SSA result), since those map 1:1 to WASM locals.
This is **not** the same as the IR's `instruction_count`, which counts
every IR instruction regardless of whether it produces a value. See §8 for
the SSA→local mapping and §9 for how `local_count` is written into the
final module.

---

## 6. Operand Encoding Rule

Per the [IR instruction encoding](./IR_DESIGN_DOC.md#instruction-encoding),
each `bal_instruction_t` is a 64-bit word with a 13-bit opcode field and three
17-bit operand fields:

```
63               51 50        34 33        17 16        00
|-----------------| |----------| |----------| |----------|
        opc             src1         src2         src3
```

The 13-bit opcode field gives 8192 possible opcodes (`OPCODE_ENUM_END = 0x7FF`
in the current enum, leaving plenty of headroom). Each 17-bit operand field
encodes either an SSA index or a constant pool index, discriminated by
`BAL_IS_CONSTANT_BIT_POSITION`:

> **The constant-discriminator bit of `src1`, `src2`, and `src3` is checked
> independently. Different operands of the same instruction can independently
> be SSA values or constant pool indices.**

This combines with the naming convention from [GloriousTacoo (4/23 5:56 PM)](https://discord.com/channels/1384639288884334693/1401595398035865762/1496993312048156893):

> **Any operand prefixed `imm` (immr, imms, imm6, imm12, ...) is a constant
> pool index. Operands like `Rd`, `Rn`, `Rm`, `Rs` are SSA variable
> indices.**

For opcodes where an operand can be either (e.g. `OPCODE_JUMP`), the IR
encodes the discriminator in the constant bit of that specific operand. The
single helper `emit_operand` handles both cases:

```c
static inline void emit_operand(BalBackendCtx*  ctx,
                                uint32_t        operand,
                                const uint64_t* constant_pool)
{
    if (operand & (1U << BAL_IS_CONSTANT_BIT_POSITION)) {
        uint32_t idx = operand & ((1U << BAL_IS_CONSTANT_BIT_POSITION) - 1);
        wasm_emit_i64_const(ctx, constant_pool[idx]);
    } else {
        wasm_emit_local_get(ctx, operand);
    }
}
```

The backend does not duplicate the discriminator check for individual
opcodes. Every operand whose source is encoding-discriminated goes through
`emit_operand`.

---

## 7. Register File in Linear Memory

Per [GloriousTacoo (4/20 5:55 PM)](https://discord.com/channels/1384639288884334693/1401595398035865762/1495905875477528687): **no import functions** — the ARM register
file lives in WASM linear memory. The backend never crosses the WASM/JS
boundary for register access.

The register file lives at the **end** of linear memory, not at offset 0.
Placing it at offset 0 would collide with guest memory accesses — a guest
load from address 0 would read X0, and a guest store to address 0 would
corrupt X0. Putting the register file at a high offset (above any address
the guest will reach) keeps the two address spaces disjoint.

Concretely, the layout is:

```
Offset 0x0000000000:               guest RAM begins (guest sees this as its address space)
…
Offset BAL_REGFILE_BASE + 0x0000:  X0 (8 bytes)
Offset BAL_REGFILE_BASE + 0x0008:  X1
…
Offset BAL_REGFILE_BASE + 0x00F8:  X31  (XZR/SP, see §7.1)
Offset BAL_REGFILE_BASE + 0x0100:  PC
Offset BAL_REGFILE_BASE + 0x0108:  SP
Offset BAL_REGFILE_BASE + 0x0110:  PSTATE
```

`BAL_REGFILE_BASE` is chosen larger than the largest address the guest can
produce. A guest load/store passes its computed address through unchanged;
the backend never adds the register-file base to guest accesses. The
mechanism for choosing and communicating `BAL_REGFILE_BASE` is open (see
§16 Q7).

`OPCODE_GET_REGISTER N` emits:

```wasm
i64.const (BAL_REGFILE_BASE + N * 8)
i64.load   align=0 offset=0
local.set  <ssa_index>
```

Strict SSA guarantees each guest register is read at most once per block via
`OPCODE_GET_REGISTER`. The backend does not verify this; the IR guarantees it.

### 7.1. X31 handling

`BAL_REGISTER_X31` is shared by XZR and SP at the encoding level — ARM64
uses register field value 31 for both, with the meaning determined by
instruction context. Disambiguation between XZR and SP happens in the **IR
layer**, not in the decoder or backend. The decoder produces an opcode
stream where the IR layer has already resolved which uses of X31 mean XZR
and which mean SP. The backend treats X31 as a regular slot at offset
`(BAL_REGFILE_BASE + 31 * 8)` and trusts the IR.

---

## 8. SSA → WASM Local Mapping

Strict SSA ([Rule 2.1 of `IR_DESIGN_DOC.md`](./IR_DESIGN_DOC.md#rule-21-single-static-assignment))
means every value-producing instruction defines exactly one immutable variable.
The mapping is implicit indexing:

```
Nth value-producing IR instruction → wasm local N
```

`local_count` increments only when the backend emits a value-producing IR
instruction. Void-result instructions (`OPCODE_STORE`, `OPCODE_RETURN`,
`OPCODE_TRAP`, `OPCODE_NOP`, `OPCODE_MOV`) do not increment it because they
do not allocate a WASM local.

Because the IR is strict SSA, no register allocator is needed. There is no
liveness analysis. The compiler embedded in `WebAssembly.compile()` will do
its own register allocation when lowering WASM locals to native registers.

All locals are declared as `i64`. Narrower types could shrink the locals
section but introduce per-opcode width tracking, deferred.

---

## 9. Locals Count Patching

The WASM binary format requires the locals declaration — including the
count of locals — to appear in the function body **before** the instruction
sequence. The decoder reads the locals declaration first to allocate
storage, then reads the instructions. The locals count must therefore
already be in the byte stream at the point the decoder gets to it.

The final value of `local_count` is only known after emission completes,
because every emitted value-producing instruction increments it. A pre-scan
over the IR array would cost a second pass — worst case 512 KB per pass.
With Tier 1 / Tier 2 running on two threads, a two-pass design becomes four
passes, 2 MB through cache per block.

The fix is patching:

```
wasm_module_begin():
  1. Emit module header through to the function body
  2. Reserve a 5-byte placeholder for the locals count
     (5 bytes = max LEB128 u32 encoding)
  3. Record placeholder offset in ctx->locals_patch_offset

[single emission pass over IR array, incrementing local_count]

wasm_module_end():
  4. Patch placeholder with ctx->local_count, padded to 5 bytes
  5. Emit `end` byte for function body
  6. Emit `end` byte for module
```

Padding the LEB128 to 5 bytes means no byte shifting is ever required: the
slot is always exactly 5 bytes, the patched value is always exactly 5
bytes, and bytes after the slot don't move.

---

## 10. WASM Module Structure

Each compiled ARM block produces one WASM module:

```
magic + version          0x00 0x61 0x73 0x6D 0x01 0x00 0x00 0x00
type section             1 function type: <signature pending §16 Q8>
import section           1 import: "env"."memory" → memory
function section         1 function, type index 0
export section           1 export: "entry" → function 0
code section
  function body
    locals declaration   <patched>: local_count locals, all i64
    instruction body     emitted during single pass
    end                  closes function body
end                      closes module
```

Exactly one import: the shared linear memory containing the ARM register
file and guest RAM. Exactly one export: `entry`. The "no imports" rule from
§7 applies to **function** imports only — the linear memory import is
required to share state with the host and is the single permitted import.

The function signature for `entry` depends on how `OPCODE_RETURN` is
ultimately wired (see §16 Q8). The two consistent choices are:

- `() → ()`: `OPCODE_RETURN` writes its result to a register file slot
  (similar to `OPCODE_MOV`) before emitting WASM `return`. The host reads
  the result from the register file after invocation.
- `() → i64`: `OPCODE_RETURN` emits its operand value onto the operand
  stack before WASM `return`, and the host receives the result as the
  function's return value.

`wasm_module_begin()` and `wasm_module_end()` run once per block, outside
the translation hot loop.

---

## 11. Assembler Layer

`assembler.h` exposes a flat set of `static inline` functions, one per WASM
operation the backend uses. Selection examples:

```c
static inline void wasm_emit_local_get (BalBackendCtx* ctx, uint32_t idx);
static inline void wasm_emit_local_set (BalBackendCtx* ctx, uint32_t idx);
static inline void wasm_emit_i64_const (BalBackendCtx* ctx, int64_t v);
static inline void wasm_emit_i64_load  (BalBackendCtx* ctx, uint32_t align, uint32_t off);
static inline void wasm_emit_i64_store (BalBackendCtx* ctx, uint32_t align, uint32_t off);
static inline void wasm_emit_i64_add   (BalBackendCtx* ctx);
static inline void wasm_emit_i64_sub   (BalBackendCtx* ctx);
static inline void wasm_emit_i64_mul   (BalBackendCtx* ctx);
static inline void wasm_emit_i64_div_s (BalBackendCtx* ctx);
static inline void wasm_emit_i64_div_u (BalBackendCtx* ctx);
static inline void wasm_emit_i64_and   (BalBackendCtx* ctx);
static inline void wasm_emit_i64_xor   (BalBackendCtx* ctx);
static inline void wasm_emit_i64_or    (BalBackendCtx* ctx);
static inline void wasm_emit_i64_shl   (BalBackendCtx* ctx);
static inline void wasm_emit_i64_shr_s (BalBackendCtx* ctx);
static inline void wasm_emit_i64_shr_u (BalBackendCtx* ctx);
static inline void wasm_emit_select    (BalBackendCtx* ctx);
static inline void wasm_emit_return    (BalBackendCtx* ctx);
static inline void wasm_emit_unreachable(BalBackendCtx* ctx);
```

Single-byte opcodes are written directly into `ctx->buf`. Anything that
requires LEB128 encoding (`local.get`, `local.set`, `i64.const`, the
load/store offset/align fields) calls into out-of-line `assembler.c`
helpers — branchy code that is not worth inlining and would bloat the
translation loop's instruction footprint.

---

## 12. Backend Mapping Layer

`backend.c` contains one function, `bal_backend_compile_block`, and one
giant switch:

```c
void bal_backend_compile_block(BalBackendCtx* ctx, const bal_ir_block_t* block)
{
    wasm_module_begin(ctx);

    uint32_t local_count = 0;

    const bal_instruction_t* cursor = block->instructions;
    const bal_instruction_t* end    = cursor + block->instruction_count;

    while (cursor < end) {
        bal_instruction_t instr = *cursor++;
        uint16_t opc  = BAL_INSTR_OPC(instr);
        uint32_t src1 = BAL_INSTR_SRC1(instr);
        uint32_t src2 = BAL_INSTR_SRC2(instr);
        uint32_t src3 = BAL_INSTR_SRC3(instr);

        switch (opc) {
            case OPCODE_GET_REGISTER: { /* §13 - register index in src1 */ }
            case OPCODE_CONST:        { /* §13 - constant pool index in src1 */ }
            /* … */
        }
    }

    ctx->local_count = local_count;
    wasm_module_end(ctx);

    ctx->callback(ctx->buf, ctx->buf_len, ctx->callback_userdata);
}
```

The switch compiles to a jump table. No indirect calls.

`emit_operand` and any other helper that needs the constant pool takes
`block->constant_pool` directly as an argument. Only opcodes whose operands
can be constant-pool indices (those with `imm` operands like `OPCODE_CONST`,
`OPCODE_SHIFT`, `OPCODE_TRAP`) ever read the constant pool; opcodes whose
operands are always SSA indices (like `OPCODE_GET_REGISTER`, where src1 is
a guest register number) do not.

---

## 13. Instruction Mapping Table

Tier 1 scope. One row per opcode confirmed implemented in the IR layer.
Operand columns reflect the operand encoding rule (§6).

For arithmetic opcodes, src2 can encode either a register (Rm, an SSA
variable index) or an immediate (constant pool index), discriminated by the
constant bit per §6. The column shows `Rm/imm` to indicate this; the helper
`emit_operand` picks the correct emission path.

| IR Opcode                   | src1 | src2       | src3        | WASM emission                                                                        | Result |
|-----------------------------|------|------------|-------------|--------------------------------------------------------------------------------------|--------|
| `OPCODE_NOP`                | -    | -          | -           | (nothing)                                                                            | void   |
| `OPCODE_GET_REGISTER`       | regN | -          | -           | `i64.const (BAL_REGFILE_BASE + regN*8)`, `i64.load align=0`, `local.set L`           | i64    |
| `OPCODE_CONST`              | imm  | -          | -           | `i64.const constant_pool[imm]`, `local.set L`                                        | i64    |
| `OPCODE_MOV` *(see §16 Q1)* | Rn   | Rd         | -           | `emit_operand src1`, `i64.const (BAL_REGFILE_BASE + Rd*8)`, `i64.store align=0`      | void   |
| `OPCODE_ADD`                | Rn   | Rm/imm     | -           | `emit_operand src1`, `emit_operand src2`, `i64.add`, `local.set L`                   | i64    |
| `OPCODE_SUB`                | Rn   | Rm/imm     | -           | `emit_operand src1`, `emit_operand src2`, `i64.sub`, `local.set L`                   | i64    |
| `OPCODE_MUL`                | Rn   | Rm/imm     | -           | `emit_operand src1`, `emit_operand src2`, `i64.mul`, `local.set L`                   | i64    |
| `OPCODE_DIV` *(see §16 Q2)* | Rn   | Rm/imm     | -           | `emit_operand src1`, `emit_operand src2`, `i64.div_s` *or* `i64.div_u`, `local.set L`| i64    |
| `OPCODE_AND`                | Rn   | Rm/imm     | -           | `emit_operand src1`, `emit_operand src2`, `i64.and`, `local.set L`                   | i64    |
| `OPCODE_XOR` (ARM `EOR`)    | Rn   | Rm/imm     | -           | `emit_operand src1`, `emit_operand src2`, `i64.xor`, `local.set L`                   | i64    |
| `OPCODE_ORN` *(see §16 Q10)*| Rn   | Rm/imm     | -           | `emit_operand src1`, `emit_operand src2`, `i64.const -1`, `i64.xor`, `i64.or`, `local.set L` | i64 |
| `OPCODE_SHIFT`              | Rn   | imms       | immr        | shift dispatch *(see §16 Q3)*                                                        | i64    |
| `OPCODE_LOAD`               | Rn   | -          | -           | `emit_operand src1` (addr), `i64.load align=0 offset=0`, `local.set L`               | i64    |
| `OPCODE_STORE`              | Rn   | Rm         | -           | `emit_operand src1` (addr), `emit_operand src2` (val), `i64.store align=0 offset=0`  | void   |
| `OPCODE_CMP`                | Rn   | Rm/imm     | -           | comparison sequence → i32 *(see §16 Q4)*                                             | i32    |
| `OPCODE_CONDITIONAL_SELECT` | cond | true_value | false_value | `emit_operand src2`, `emit_operand src3`, `local.get src1`, `select`, `local.set L`  | i64    |
| `OPCODE_TRAP`               | imm  | -          | -           | `unreachable` *(see §16 Q5)*                                                         | void   |
| `OPCODE_RETURN` *(see Q6,Q8)* | Rn | -          | -           | depends on §10 function signature *(see §16 Q8)*                                     | void   |

In the table above, `L` denotes the implicit SSA index of the current
instruction, materialized as the next unused WASM local (see §8).

**`OPCODE_ORN` lowering note.** WASM has no ORN instruction. ARM64 ORN
computes `Rn | (~Rm)` (ignoring the optional shift, see §16 Q10). Emitted
as: load Rn, load Rm, XOR Rm with -1 to invert it, OR with Rn. Five WASM
ops. Cheap and correct for the unshifted case.

**`OPCODE_LOAD` / `OPCODE_STORE` alignment note.** ARM64 permits unaligned
memory access. The emitter passes `align=0` (the WASM "no alignment
requirement" hint) on all guest memory accesses. The WASM engine may
internally optimize aligned accesses where it can prove alignment, but the
emitter does not impose a requirement.

**`OPCODE_CONDITIONAL_SELECT` note.** Per [Rule 1.3](./IR_DESIGN_DOC.md#rule-13-opcode_if-and-opcode_conditional_select)
and the [If-to-Select pass](./IR_DESIGN_DOC.md#if-to-select-optimization-pass),
this opcode receives the speculatively-executed values from both arms in
src2 and src3, with the condition in src1. WASM `select` (opcode `0x1B`)
takes `(value_true, value_false, condition_i32)` and returns the true value
when the condition is non-zero. Maps cleanly to ARM `CSEL`.

---

## 14. Out of Scope

Per direction from [GloriousTacoo (4/23 6:29 PM)](https://discord.com/channels/1384639288884334693/1401595398035865762/1497001614127399002), the following are
present in `bal_opcode_t` but **explicitly excluded** from this revision.
They will be added once the IR translation logic stabilizes for them and
the tiered translation loop rewrite is complete:

- Branching opcodes: `OPCODE_JUMP`, `OPCODE_BRANCH_ZERO`,
  `OPCODE_BRANCH_NOT_ZERO`, `OPCODE_TEST_BIT_ZERO`
- Host calls: `OPCODE_CALL_HOST`
- Scope-based control flow: `OPCODE_IF`, `OPCODE_LOOP`, `OPCODE_BREAK`,
  `OPCODE_CONTINUE`, `OPCODE_MERGE`, `OPCODE_YIELD`, `OPCODE_BLOCK_ARG`,
  `OPCODE_ARG_EXTENSION`
- Tier 2 optimized translation
- The host-callback mechanism (only relevant once branches and traps need it)

When these are finalized, this document will be extended, not rewritten.
The Tier 1 emitter for the opcodes above does not depend on any of them.

---

## 15. Planned: Copy Elision

When `ssa_use_counts[]` lands on the IR side (per [GloriousTacoo, 4/20 6:17 PM](https://discord.com/channels/1384639288884334693/1401595398035865762/1495911344476782633)),
the backend can elide redundant `local.get`/`local.set` pairs for SSA values
used exactly once. The optimization is:

```
If ssa_use_counts[ssa] == 1:
    Skip the trailing `local.set` on the producer.
    Skip the leading `local.get` on the single consumer.
    The value sits on the WASM operand stack between the two.
```

Saves roughly 4–8 bytes per single-use SSA value, plus a `local_count`
allocation. Not implemented in Tier 1. Added to the design here so the
emission code is structured to support it without refactoring later — every
producer's `local.set` and every consumer's `local.get` go through helper
functions that can become no-ops when use-count is 1.

**Constraint when control flow lands.** Copy elision across an `OPCODE_IF`
or `OPCODE_LOOP` boundary is not safe. WASM's stack discipline requires
the operand stack to be empty at structured-control-flow boundaries unless
the block's signature explicitly declares stack inputs/outputs. A value
yielded out of an IF arm therefore must be `local.set` before the `end`
opcode and `local.get` on the merge side. The use-count check must be
gated on "producer and consumer share the same scope depth" once IF/LOOP
land.

---

## 16. Open Questions

These are the gaps the Discord thread did not close, plus questions
surfaced during review of Draft 1. They block finalizing the table in §13
but do not block starting the assembler layer or the module builder.

**Q1 — `OPCODE_MOV` emission confirmation.** Per review feedback, `Rd` is a
guest register index (not an SSA variable index). The assumed emission is
`emit_operand src1; i64.const (BAL_REGFILE_BASE + Rd*8); i64.store align=0 offset=0`
— load the SSA value referenced by Rn, store it to the register file slot
for the guest register Rd. Need confirmation this matches the intended
semantic, or a correction if MOV is something else.

**Q2 — `OPCODE_DIV` signed/unsigned split.** Confirmed planned on
[4/23 6:29 PM](https://discord.com/channels/1384639288884334693/1401595398035865762/1497001614127399002),
not yet committed — the current enum has only `OPCODE_DIV`. Once the split
lands (presumably as `OPCODE_DIV_S` and `OPCODE_DIV_U`), §13 splits into
two rows. Until then, `OPCODE_DIV` cannot be emitted correctly because the
sign is not encoded.

**Q3 — `OPCODE_SHIFT` direction encoding.** From [4/21 6:39 AM](https://discord.com/channels/1384639288884334693/1401595398035865762/1496098061078892614):
imms and immr are encoded into src2 and src3 as constant pool indices,
with src1 as the SSA source register. The transcript covers `LSL (Immediate)` only.
Need confirmation of how `LSR`, `ASR`, `ROR` are distinguished — separate
opcodes, a tag in the constant pool entry, or another `imm` field in the
instruction encoding.

**Q4 — `OPCODE_CMP` result encoding.** Confirmed ([4/21 6:39 AM](https://discord.com/channels/1384639288884334693/1401595398035865762/1496098061078892614))
to produce an SSA value. Open: is the result a 1-bit `i32` boolean
(matching WASM comparison ops directly), an `i64` 0/1 value, or the full
ARM PSTATE flag set packed into an integer? The emission sequence depends
on which. Leaning toward i32 boolean since that's what WASM `select`
consumes for `OPCODE_CONDITIONAL_SELECT`.

**Q5 — `OPCODE_TRAP` notification mechanism.** Per the no-imports rule
(§7), an import call is not an option. The current Tier 1 mapping is
`unreachable`, which terminates the WASM module but loses the trap reason.
A non-import mechanism that preserves the reason is needed. Candidates:

- Write the reason to a fixed location in linear memory before
  `unreachable` (host inspects after catching the trap)
- Use a WASM global to communicate the reason (requires the host to set up
  a global at module instantiation)
- Encode the reason in the trap location and have the host inspect the
  WASM source map

Need direction on which mechanism the engine wants.

**Q6 — `OPCODE_RETURN` semantics under inlining.**
[`IR_DESIGN_DOC.md`](./IR_DESIGN_DOC.md#instruction-set-architecture)
explicitly flags `OPCODE_RETURN` as undecided: *"Not exactly sure about
this one yet. How would function inlining work?"* The current Tier 1 plan
follows GloriousTacoo's 4/20 5:55 PM direction ("Always map OPCODE_RETURN
into a WASM return"), but this needs revisiting when function inlining
lands — an inlined RETURN cannot emit a WASM `return` because that exits
the entire compiled block, not just the inlined function.

**Q7 — `BAL_REGFILE_BASE` definition and management.** §7 introduces
`BAL_REGFILE_BASE` as the offset where the register file lives in linear
memory, but doesn't specify:

- Where it's defined (compile-time `#define`, runtime parameter,
  host-supplied at module instantiation, WASM global imported by the
  module?)
- How its value is chosen (largest possible guest address from the
  binary's linker map, fixed top-of-memory offset, dynamic per-block?)
- Whether it's the same across all compiled blocks or can vary
- How emitted modules reference it — is the value baked into emitted
  `i64.const` instructions at compile time (which means recompilation if
  the value changes), or read from a WASM global at runtime?

The choice affects whether changes to the register-file location
invalidate cached compiled modules.

**Q8 — Function signature and `OPCODE_RETURN` emission.** §10 originally
specified the WASM function type as `() → ()`, but `OPCODE_RETURN` taking
`Rn` (per Q6 review feedback) implies a return value. The two are
inconsistent. Two consistent designs:

- **`() → ()` with register-file write.** `OPCODE_RETURN` writes its
  operand to a designated register file slot (e.g., the X0 slot, since
  ARM64 returns in X0) before emitting WASM `return`. The host reads the
  result from the register file after invocation.
- **`() → i64` with operand-stack return.** `OPCODE_RETURN` emits its
  operand value onto the operand stack before WASM `return`, and the host
  receives the result as the function's return value.

The first is more uniform with the rest of the design (everything goes
through the register file). The second is more idiomatic WASM. Need
direction on which to use.

**Q9 — Arithmetic with immediates in src2.** §13 shows `Rm/imm` in src2
for arithmetic opcodes, with `emit_operand` handling the discrimination.
Question: does the `imm` form ever require additional handling beyond
loading the constant pool entry as an `i64.const` (e.g., sign extension,
shift for ARM's bitmask immediates) before the arithmetic op, or is it
always a clean i64 value?

**Q10 — `OPCODE_ORN` and shifts.** ARM64 ORN is `Rd = Rn | (NOT shifted_Rm)`.
The current §13 lowering handles the unshifted case (`Rn | ~Rm`). Open:
does the IR's `OPCODE_ORN` carry shift information, or is shifting
expressed as a separate `OPCODE_SHIFT` whose result is consumed by
`OPCODE_ORN`? The latter would compose cleanly with the existing IR; the
former would require additional emission logic in the ORN row.

---

## 17. Limitations

**65,536 instruction ceiling.** Inherited from the IR's 17-bit operand
encoding. The IR layer splits oversized blocks before they reach the
backend.

**All locals declared as i64.** `ssa_bit_widths[]` exists but using narrower
types is deferred. Costs roughly 1 byte per local in the locals section,
trivial.

**Async compilation latency.** `WebAssembly.compile()` is asynchronous. The
CPU worker runs through an interpreter until the compiled module is ready.
For hot blocks the gap closes after one compilation. This is a host-side
concern, not a backend-side concern, but worth noting since the design
assumes the host has an interpreter fallback.

**Module cache invalidation under IR compaction.** Per
[Rule 4.1 of `IR_DESIGN_DOC.md`](./IR_DESIGN_DOC.md#rule-41-deleted-instructions),
the IR layer triggers a Compaction Pass that remaps SSA indices when dead
NOPs exceed 25%. After compaction, the same guest address can produce
different WASM bytes than the cached module from before compaction. The
host-side module cache must therefore key on `(guest_address, ir_build_id)`
or similar, not on `guest_address` alone. This is a host concern but the
backend triggers the condition.

**No SIMD / FP.** Tier 1 is integer-only. Q-register support, FP rounding,
and NEON come whenever the IR adds them.
