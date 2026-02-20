<div align="center">

# 🌻 Sunflower — Architecture

### Internal design of the Sunflower language runtime and the **FISH** bytecode VM

*Flower Instruction Set Hybrid — specification, VM internals, and memory model*

---

</div>

## Table of Contents

1. [High-Level Architecture](#1-high-level-architecture)
2. [Compilation Pipeline](#2-compilation-pipeline)
3. [Lexer](#3-lexer)
4. [Abstract Syntax Tree](#4-abstract-syntax-tree)
5. [The FISH Instruction Set](#5-the-fish-instruction-set)
6. [FISH Instruction Reference](#6-fish-instruction-reference)
7. [VM Internals](#7-vm-internals)
8. [Object System & Memory Management](#8-object-system--memory-management)
9. [Hash Table Implementation](#9-hash-table-implementation)
10. [Concurrency Model](#10-concurrency-model)
11. [Code Generation](#11-code-generation)
12. [FISH Bytecode Decomposition Examples](#12-fish-bytecode-decomposition-examples)
13. [Performance Considerations](#13-performance-considerations)
14. [Design Tradeoffs](#14-design-tradeoffs)
15. [Appendix: Constants & Limits](#15-appendix-constants--limits)

---

## 1. High-Level Architecture

Sunflower is structured as a linear pipeline transforming source text into executable bytecode, with an alternative AST-interpreted path for direct execution.

```
┌──────────┐    ┌─────────┐    ┌──────────┐    ┌─────────────┐    ┌──────────┐
│  Source   │───▶│  Lexer  │───▶│  Parser  │───▶│   Codegen   │───▶│ FISH VM  │
│  (.sf)   │    │ token.c │    │  ast.c   │    │  codegen.c  │    │bytecode.c│
└──────────┘    └─────────┘    └──────────┘    └─────────────┘    └──────────┘
                  tokens         StmtSM           instr_t[]         execution
                (TokenSM)      (stmt_t[])                          (vm_t)
                                    │
                                    ▼ (alternative path)
                              ┌──────────┐
                              │AST Interp│
                              │ parser.c │
                              └──────────┘
```

### Execution Modes

| Mode | Entry Point | Description |
|---|---|---|
| **FISH Bytecode VM** | `sf_vm_exec_frame_top()` | Compiles AST to FISH instructions, then executes on the stack-based VM. This is the primary, performance-oriented path. |
| **AST Interpreter** | `sf_parser_exec()` | Walks the AST directly, evaluating expressions and statements via recursive tree traversal. Simpler but slower; useful for bootstrapping and debugging. |

### Memory Model

- **Deterministic**: atomic reference counting (`IR`/`DR` macros) — no tracing GC, no pauses.
- **Object store**: global pool with free-list reuse, mutex-protected allocation.
- **Cached constants**: small integers (-5 to 255), empty string `""`, and `none` are pre-allocated and never freed.
- **Platform abstraction**: `sfmutex_t` wraps `pthread_mutex_t` (Unix) or `HANDLE` (Win32).

### Value Kinds

All runtime values are represented as `obj_t` (tagged union):

| `obj_t.type` | Payload | Description |
|---|---|---|
| `OBJ_CONST` | `const_t` with `CONST_INT`, `CONST_FLOAT`, `CONST_STRING`, `CONST_BOOL`, `CONST_NONE` | Immutable scalar values |
| `OBJ_FUNC` | `fun_t` with `FUN_NATIVE` or `FUN_CODED` | Callable functions |
| `OBJ_CLASS` | `class_t` with parallel `slots[]`/`vals[]` | Property-bag class instances |

---

## 2. Compilation Pipeline

### Stage 1: Lexing ([token.c](token.c), [token.h](token.h))

Source text → `TokenSM` (token state machine) containing an array of `token_t` values.

### Stage 2: Parsing ([ast.c](ast.c), [ast.h](ast.h))

Token stream → `StmtSM` (statement state machine) containing an array of `stmt_t` values. The parser is indentation-sensitive, using `TOK_SPACE` tokens to detect block boundaries.

### Stage 3: Code Generation ([codegen.c](codegen.c), [codegen.h](codegen.h))

`StmtSM` → FISH instruction stream (`instr_t[]`) written into `vm_t.insts`. The compiler resolves variables to numbered slots (global/local/name), manages scope via a hash table stack, and performs constant folding.

### Stage 4: Execution ([bytecode.c](bytecode.c), [bytecode.h](bytecode.h))

The VM fetches instructions from `vm_t.insts[ip]`, dispatches via `switch(i.op)`, and executes against the value stack, frame stack, and global/local storage.

---

## 3. Lexer

**Files:** [token.h](token.h), [token.c](token.c)

### Token Types

| Enum | Value | Description |
|---|---|---|
| `TOK_NEWLINE` | 0 | Line terminator |
| `TOK_SPACE` | 1 | Indentation (tracks `size_t` count for block detection) |
| `TOK_INTEGER` | 2 | Integer literal (C `int`) |
| `TOK_FLOAT` | 3 | Float literal (C `float`) |
| `TOK_STRING` | 4 | String literal (heap-allocated `char*`, supports escape sequences) |
| `TOK_OPERATOR` | 5 | Operators: `+` `-` `*` `/` `=` `(` `)` `.` `,` `==` `!=` `<` `>` `<=` `>=` |
| `TOK_IDENTIFIER` | 6 | Variable/function names |
| `TOK_KEYWORD` | 7 | Reserved keywords (see below) |
| `TOK_BOOL` | 8 | `True` / `False` |
| `TOK_NONE` | 9 | `None` |
| `TOK_EOF` | 10 | End of input |

### Reserved Keywords (21)

```
if    else    for      while    in       to       step
class fun     return   or       and      not      repeat
import as     try      catch    break    continue extends
```

### Lexer Architecture

```
TokenSM {
    char *raw;         // raw source text
    size_t rl;         // raw length
    token_t *vals;     // output token array
    size_t vl;         // current length
    size_t vc;         // capacity (initial: 64)
}
```

`sf_token_gen()` iterates character-by-character, dispatching to specialized handlers for identifiers, numbers (int/float), strings (with `\n`, `\t`, `\\`, `\"` escape processing), operators, and whitespace. Tokens are appended to `vals` with automatic resize via `sf_token_resize()`.

---

## 4. Abstract Syntax Tree

**Files:** [ast.h](ast.h), [ast.c](ast.c), [expr.h](expr.h), [expr.c](expr.c), [stmt.h](stmt.h), [stmt.c](stmt.c)

### Expression Types (`expr_t`)

| Enum | Fields | Description |
|---|---|---|
| `EXPR_CONST` | `const_t v` | Literal value (int, float, string, bool, none) |
| `EXPR_VAR` | `const char *v` | Variable reference by name |
| `EXPR_ADD_1` | `expr_t *v` | Optimized increment: `v + 1` |
| `EXPR_ARITHMETIC` | `arith_node_t *tree`, `size_t tl` | Postfix arithmetic tree (shunting-yard output) |
| `EXPR_FUNCALL` | `expr_t *name`, `expr_t **args`, `size_t al` | Function call with argument list |
| `EXPR_CMP` | `int type`, `expr_t *left`, `expr_t *right` | Comparison (6 types: `==` `!=` `<` `>` `<=` `>=`) |
| `EXPR_DOT_ACCESS` | `expr_t *left`, `char *right` | Member access: `left.right` |

### Statement Types (`stmt_t`)

| Enum | Fields | Description |
|---|---|---|
| `STMT_VARDECL` | `expr_t *name`, `expr_t *val` | Variable declaration/assignment |
| `STMT_FUNCALL` | `expr_t *name`, `expr_t **args`, `size_t argc` | Function call as statement (return value discarded) |
| `STMT_IFBLOCK` | `expr_t *cond`, `stmt_t *body/else_body`, `size_t bl/else_bl` | Conditional with optional else branch |
| `STMT_WHILE` | `expr_t *cond`, `stmt_t *body`, `size_t bl` | While loop |
| `STMT_FUNDECL` | `const char *name`, `expr_t **args`, `size_t argc`, `stmt_t *body`, `size_t bl` | Function definition |
| `STMT_RETURN` | `expr_t *v` | Return statement |
| `STMT_CLASSDECL` | `const char *name`, `stmt_t *body`, `size_t bl` | Class definition |
| `STMT_EOF` | — | End-of-file sentinel |

### Comparison Types (`CmpType`)

| Enum | Source Operator |
|---|---|
| `CMP_EQEQ` | `==` |
| `CMP_NEQ` | `!=` |
| `CMP_LE` | `<` |
| `CMP_GE` | `>` |
| `CMP_LEQ` | `<=` |
| `CMP_GEQ` | `>=` |

### Parser Architecture

The parser in [ast.c](ast.c) is **indentation-sensitive**:

1. **Block detection**: `TOK_SPACE` tokens at the start of lines are compared against a tracked indentation level. Increased indentation opens a new block; returning to a previous level closes it.
2. **Statement dispatch**: Based on the first meaningful token — `TOK_KEYWORD` dispatches to `if/while/fun/class/return` handlers; `TOK_IDENTIFIER` followed by `=` is a variable declaration; `TOK_IDENTIFIER` followed by `(` is a function call.
3. **Expression parsing**: `sf_expr_gen()` handles constants, variables, function calls (recursive descent for arguments), dot-access chains, and arithmetic expressions.

### Arithmetic Processing

Arithmetic expressions go through a multi-stage pipeline:

```
Infix tokens  ──▶  arith_node_t[] (infix)  ──▶  postfix (shunting-yard)  ──▶  constant fold
                                                                                    │
                                                                              EXPR_CONST (if all constant)
                                                                              or EXPR_ARITHMETIC (if variables present)
```

- **Shunting-yard** (`sf_arith_makepostfix`): converts infix `arith_node_t` array to postfix in-place using operator precedence (`sf_arith_getprec`).
- **Constant folding** (`sf_arith_eval_consttree`): if all operands are `EXPR_CONST`, the tree is evaluated at parse time and collapsed to a single `EXPR_CONST`.
- **Add-one optimization**: the pattern `expr + 1` is detected and collapsed to `EXPR_ADD_1(expr)`, which compiles to the dedicated `OP_ADD_1` FISH opcode.

---

## 5. The FISH Instruction Set

**FISH** (_Flower Instruction Set Hybrid_) is Sunflower's purpose-built bytecode instruction set. It defines the contract between the compiler ([codegen.c](codegen.c)) and the virtual machine ([bytecode.c](bytecode.c)).

### Design Goals

1. **Minimalism** — 21 opcodes are sufficient to compile all supported language constructs.
2. **Stack orientation** — operands flow through a value stack, simplifying compilation from tree-structured ASTs.
3. **Fixed format** — every instruction is `instr_t { opcode_t op; int a; int b; char *c; }`, making disassembly, serialization, and debugging straightforward.
4. **Scope-aware storage** — three distinct storage namespaces (global slots, frame-local slots, name-scope slots) allow the compiler to emit direct slot access without runtime name lookups during execution.
5. **Optimization-friendly** — dedicated opcodes like `OP_ADD_1` and the constant pool allow common patterns to execute with fewer instructions.

### Instruction Format

```c
typedef struct _inst_s {
    opcode_t op;    // Operation code (21 values)
    int      a;     // Primary operand (slot index, const index, IP target, arg count, etc.)
    int      b;     // Secondary operand (arity, depth, keep-return flag, etc.)
    char    *c;     // String operand (member name for DOT_ACCESS, slot name for STORE_NAME)
} instr_t;
```

Not all fields are used by every opcode. Unused fields are zero/NULL.

### Storage Scopes

| Scope | Compiled As | Runtime Storage | Use Case |
|---|---|---|---|
| **Global** (`SF_VM_SLOT_GLOBAL`) | `OP_LOAD` / `OP_STORE` with slot `a` | `vm->globals[a]` (pre-allocated array of 512) | Top-level variables and functions |
| **Local** (`SF_VM_SLOT_LOCAL`) | `OP_LOAD_FAST` / `OP_STORE_FAST` with slot `a` | `frame->l.locals[a]` (per-frame array, capacity 64) | Function parameters and local variables |
| **Name** (`SF_VM_SLOT_NAME`) | `OP_LOAD_NAME` / `OP_STORE_NAME` with slot `a` | `frame->n.names[a]` / `frame->n.vals[a]` | Class body construction slots |

---

## 6. FISH Instruction Reference

### Load Operations

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_LOAD_CONST` | `a` = const pool index | +1 | Push `vm->map_consts[a]` wrapped in an `obj_t` onto the stack. The constant is retrieved from the object store with caching for small ints, empty string, and none. |
| `OP_LOAD` | `a` = global slot | +1 | Push `vm->globals[a]` onto the stack. Increments the object's reference count. |
| `OP_LOAD_FAST` | `a` = local slot, `b` = depth | +1 | Push a local variable. When `b=0`, reads from the current frame's `locals[a]`. When `b>0`, walks `b` frames backward to support lexical variable access across nested function scopes. |
| `OP_LOAD_NAME` | `a` = name slot | +1 | Push a name-scope variable from the current `FRAME_NAME`'s `vals[a]`. Used inside class body construction. |
| `OP_LOAD_FUNC_CODED` | `a` = entry IP, `b` = arity | +1 | Creates a new `fun_t` with `type = FUN_CODED`, sets the function's entry point to instruction `a` and expected argument count to `b`. Wraps it in an `obj_t` and pushes onto the stack. |

### Store Operations

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_STORE` | `a` = global slot | −1 | Pop the top of stack and write to `vm->globals[a]`. Decrements the reference count of the previous occupant (if any) and increments the new value's count. |
| `OP_STORE_FAST` | `a` = local slot | −1 | Pop the top of stack and write to the current frame's `locals[a]`. Handles reference counting for the previous and new values. |
| `OP_STORE_NAME` | `a` = name slot, `c` = slot name | −1 | Pop the top of stack and write to the current `FRAME_NAME`'s `vals[a]` with the associated name stored as `names[a] = c`. Used during class body construction. |

### Arithmetic Operations

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_ADD_1` | — | 0 (pop 1, push 1) | Pop one integer, push `value + 1`. This is a **dedicated optimization** for the common increment pattern `x + 1`, compiled from `EXPR_ADD_1`. Avoids the overhead of loading a constant `1` and performing a generic add. |
| `OP_ADD` | — | −1 (pop 2, push 1) | Pop two values, push their integer sum. |
| `OP_SUB` | — | −1 (pop 2, push 1) | Pop two values, push `second_popped - first_popped`. |
| `OP_MUL` | — | −1 (pop 2, push 1) | Pop two values, push their integer product. |
| `OP_DIV` | — | −1 (pop 2, push 1) | Pop two values, push `second_popped / first_popped` (integer division). |

> **Note:** `OP_ADD_1` has a net stack effect of **0** (pops one, pushes one) — it is unary, unlike the binary arithmetic opcodes which have a net effect of −1.

### Comparison

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_CMP` | `a` = `CmpType` enum value | −1 (pop 2, push 1) | Pop two values, compare according to `a` (`CMP_EQEQ`, `CMP_NEQ`, `CMP_LE`, `CMP_GE`, `CMP_LEQ`, `CMP_GEQ`). Push a boolean `obj_t` result. |

### Control Flow

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_JUMP` | `a` = target IP | 0 | Set `vm->ip = a`. Unconditional branch used for loop back-edges and skipping function bodies. |
| `OP_JUMP_IF_FALSE` | `a` = target IP | −1 | Pop the top of stack. If the value is falsey (checked via `sf_obj_isfalse`), set `vm->ip = a`. Otherwise, fall through. Used for `if` conditions and `while` loop guards. |

### Function Calls

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_CALL` | `a` = arg count, `b` = keep return | −(a+1), then +(b) | Pop the callee object and `a` argument objects from the stack. Dispatch based on function type: **Native** (`FUN_NATIVE`): call the C function pointer directly based on `nf_type` (`NF_ARG_1`, `NF_ARG_2`, `NF_ARG_3`, or `NF_ARG_ANY`). If `scc` (system code call) is set, the native function is invoked via an optimized fast path. **Coded** (`FUN_CODED`): push a new `FRAME_LOCAL`, bind arguments to local slots, save `return_ip = current_ip`, set `pop_ret_val` based on `b`, and jump to the function's entry label. If `b=1`, the return value is kept on the stack; if `b=0`, it is discarded (statement-level call). |

### Class Construction

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_LOAD_BUILDCLASS` | `a` = matching `END` IP | 0 | Enter class construction mode. Pushes a new `FRAME_NAME` onto the frame stack with empty `names[]`/`vals[]` arrays. Execution continues into the class body, where `OP_STORE_NAME` instructions populate the slots. |
| `OP_LOAD_BUILDCLASS_END` | `a` = matching `START` IP | +1 | Exit class construction mode. Reads all accumulated name/value pairs from the current `FRAME_NAME`, creates a `class_t` with the collected slots, wraps it in an `obj_t`, and pushes it onto the stack. Pops the name frame. |

### Member Access

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_DOT_ACCESS` | `c` = member name | 0 (pop 1, push 1) | Pop the top of stack (expected to be an `OBJ_CLASS`). Look up the member named `c` in the class's `slots[]` array via `container_access()`. Push the found member value. Asserts if the member is not found. |

### Frame Control

| Opcode | Operands | Stack Effect | Description |
|---|---|---|---|
| `OP_RETURN` | `a` = explicit flag | varies | Terminate the current frame. If `a=1` (explicit `return` statement), the return value is already on the stack. If `a=0` (implicit return at end of function/module), a `none` object is pushed. The frame is then popped, stack is restored to `stack_base`, and `ip` is set to `return_ip`. If `pop_ret_val` is set on the frame, the return value is discarded after restoration. |

---

## 7. VM Internals

**Files:** [bytecode.h](bytecode.h), [bytecode.c](bytecode.c)

### VM State (`vm_t`)

```c
typedef struct _vm_s {
    // Instruction stream
    size_t    ip;           // Instruction pointer
    instr_t  *insts;       // FISH instruction array
    size_t    inst_len;    // Current instruction count
    size_t    inst_cap;    // Instruction array capacity

    // Constant pool
    const_t  *map_consts;  // Constant table (indexed by OP_LOAD_CONST operand)
    size_t    s_ml;        // Constant count
    size_t    s_mc;        // Constant capacity

    // Scope hash tables (used during codegen for symbol resolution)
    hashtable_t **hts;     // Hash table stack
    size_t    htl;         // Stack depth
    size_t    htc;         // Stack capacity

    // Global storage
    obj_t   **globals;     // Global variable slots (capacity: 512)
    size_t    globals_cap;

    // Value stack
    obj_t   **stack;       // Operand stack (capacity: 128)
    size_t    stack_cap;
    size_t    sp;          // Stack pointer (next free slot)

    // Call frames
    frame_t  *frames;      // Frame stack (capacity: 500)
    size_t    fp;          // Frame pointer (current frame index)
    size_t    frame_cap;

    // Codegen metadata
    struct {
        int    slot;       // Current scope type (GLOBAL/LOCAL/NAME)
        size_t g_slot;     // Next global slot number
        size_t l_slot;     // Next local slot number
        size_t n_slot;     // Next name slot number
    } meta;
} vm_t;
```

### Frame Structure (`frame_t`)

```c
typedef struct _frame_s {
    int    type;           // FRAME_LOCAL or FRAME_NAME
    size_t return_ip;      // IP to resume after RETURN

    union {
        struct {           // FRAME_LOCAL
            obj_t **locals;       // Local variable array
            size_t  locals_cap;   // Capacity (initial: 64)
            size_t  locals_count; // Occupied slots
        } l;

        struct {           // FRAME_NAME (class construction)
            char  **names;        // Slot name array
            obj_t **vals;         // Slot value array
            size_t  nvl;          // Count
            size_t  nvc;          // Capacity
        } n;
    };

    size_t stack_base;     // Stack pointer at frame entry (for restoration)
    int    pop_ret_val;    // Whether to discard return value (statement-level calls)
} frame_t;
```

### Execution Loop

The core execution loop in `sf_vm_exec_frame_top()` follows this pattern:

```c
while (1) {
    instr_t i = vm->insts[vm->ip];
    switch (i.op) {
        case OP_LOAD_CONST:  /* push constant */        break;
        case OP_LOAD:        /* push global */           break;
        case OP_LOAD_FAST:   /* push local */            break;
        case OP_STORE:       /* pop → global */          break;
        case OP_STORE_FAST:  /* pop → local */           break;
        case OP_CALL:        /* invoke function */       break;
        case OP_ADD_1:       /* optimized increment */   break;
        case OP_ADD:         /* binary add */            break;
        case OP_SUB:         /* binary subtract */       break;
        case OP_MUL:         /* binary multiply */       break;
        case OP_DIV:         /* binary divide */         break;
        case OP_JUMP:        /* unconditional branch */  break;
        case OP_JUMP_IF_FALSE: /* conditional branch */  break;
        case OP_CMP:         /* comparison → bool */     break;
        case OP_LOAD_FUNC_CODED: /* create function */   break;
        case OP_LOAD_BUILDCLASS: /* enter class frame */ break;
        case OP_LOAD_BUILDCLASS_END: /* build class */   break;
        case OP_STORE_NAME:  /* pop → name slot */       break;
        case OP_LOAD_NAME:   /* push name slot */        break;
        case OP_DOT_ACCESS:  /* member lookup */         break;
        case OP_RETURN:      /* unwind frame */          break;
    }
    vm->ip++;
}
```

### Call Mechanics

When `OP_CALL` is encountered:

```
Stack before:  [ ... | callee | arg0 | arg1 | ... | argN-1 ]
                       ↑ sp-(N+1)                    ↑ sp-1
```

1. Pop `a` arguments and the callee from the stack.
2. **If native (`FUN_NATIVE`):**
   - Dispatch by `nf_type`:
     - `NF_ARG_1`: `callee->v.native.v.f_onearg(arg0)`
     - `NF_ARG_2`: `callee->v.native.v.f_twoarg(arg0, arg1)`
     - `NF_ARG_3`: `callee->v.native.v.f_threearg(arg0, arg1, arg2)`
     - `NF_ARG_ANY`: `callee->v.native.v.f_anyarg(args, argc)`
   - If `scc` flag is set, the function is a system code call and skips argument object creation overhead.
   - If `b=1`, push the return value; if `b=0`, discard it.
3. **If coded (`FUN_CODED`):**
   - Create a new `FRAME_LOCAL` via `sf_frame_new_local()`.
   - Bind arguments to `frame.l.locals[0..N-1]` with reference count increments.
   - Set `frame.return_ip = current_ip` and `frame.stack_base = sp`.
   - Set `frame.pop_ret_val = !b` (discard return if statement-level call).
   - Push frame via `sf_vm_addframe()`.
   - Set `vm->ip = callee->v.coded.lp - 1` (jump to function entry; `-1` because the loop increments).

### Return Mechanics

When `OP_RETURN` is encountered:

1. If `a=0` (implicit return): push `none` onto the stack.
2. If `a=1` (explicit return): return value is already on the stack.
3. Save the return value from stack top.
4. Pop the current frame via `sf_vm_popframe()`.
5. Restore `sp` to `frame.stack_base`.
6. Push the return value back.
7. Set `ip = frame.return_ip`.
8. If `frame.pop_ret_val == 1`, pop and discard the return value.

### Lexical Depth Access (`OP_LOAD_FAST` with `b > 0`)

When a function references a variable from an enclosing scope, the compiler emits `OP_LOAD_FAST` with `b` set to the number of frames to walk backward:

```
Frame 3 (current): b=0 → read locals[a] from frame 3
                    b=1 → read locals[a] from frame 2
                    b=2 → read locals[a] from frame 1
```

This provides limited lexical scoping without full closures/upvalues. The referenced frame must still be on the frame stack (i.e., still executing).

---

## 8. Object System & Memory Management

**Files:** [object.h](object.h), [object.c](object.c), [fun.h](fun.h), [fun.c](fun.c), [cl.h](cl.h), [cl.c](cl.c), [malloc.h](malloc.h), [malloc.c](malloc.c)

### Object Structure

```c
typedef struct object_s {
    int type;                   // OBJ_CONST, OBJ_FUNC, or OBJ_CLASS

    union {
        struct { const_t v; } o_const;      // Scalar constant
        struct { fun_t *v;  } o_fun;        // Function (native or coded)
        struct { class_t *v; } o_class;     // Class instance
    } v;

    struct {
        atomic_int ref_count;   // Atomic reference count
        int        active;      // 1 = in use, 0 = free-list
        int64_t    index;       // Slot index in the global object store
    } meta;
} obj_t;
```

### Object Store

The global object store is a flat array of `obj_t` managed as a free-list:

```
┌──────────────────────────────────────────────────────────┐
│ obj_t[0]   obj_t[1]   obj_t[2]   ...   obj_t[N-1]       │
│ (active)   (free)     (active)         (free)            │
└──────────────────────────────────────────────────────────┘
       ↓                                      ↓
  In-use object                        Free-list entry
```

- **Allocation** (`sf_objstore_req`): locks the store mutex, scans for a free slot (`active == 0`), returns it. If no free slots, grows the store array.
- **Deallocation** (`sf_obj_free`): marks the slot as inactive (`active = 0`), returning it to the free-list pool. For `OBJ_FUNC` with `FUN_CODED`, the `fun_t` payload is freed.
- **Thread safety**: a `sfmutex_t` protects all store mutations.

### Cached Constants

On initialization (`sf_objstore_init`), the store pre-allocates and permanently caches:

| Cached Value | Count | Purpose |
|---|---|---|
| Integers -5 to 255 | 261 objects | Avoids allocation for the most common integer values |
| Empty string `""` | 1 object | Common sentinel / default |
| `none` | 1 object | Null/void representation |

`sf_objstore_req_forconst()` checks incoming constants against the cache and returns the pre-existing object when matched, completely avoiding allocation.

### Reference Counting

```c
// Increment (relaxed ordering — no synchronization barrier needed for inc)
#define IR(X) { sf_obj_rc_inc(X); }
void sf_obj_rc_inc(obj_t *o) {
    atomic_fetch_add_explicit(&o->meta.ref_count, 1, memory_order_relaxed);
}

// Decrement (acquire-release ordering — ensures all prior writes visible before potential free)
#define DR(X) { sf_obj_rc_dec(X); }
void sf_obj_rc_dec(obj_t *o) {
    if (atomic_fetch_sub_explicit(&o->meta.ref_count, 1, memory_order_acq_rel) == 1) {
        sf_obj_free(o);  // ref_count reached zero → reclaim
    }
}
```

**Memory ordering rationale:**
- `memory_order_relaxed` for increments: safe because incrementing can never trigger a free.
- `memory_order_acq_rel` for decrements: the decrementing thread must see all prior writes to the object before potentially freeing it.

### Function Representation (`fun_t`)

```c
typedef struct {
    char  *name;    // Function name
    int    type;    // FUN_NATIVE or FUN_CODED

    char **args;    // Parameter names
    size_t argl;    // Argument count
    size_t argc;    // Argument capacity

    union {
        struct {                           // FUN_NATIVE
            int nf_type;                   // NF_ARG_1, NF_ARG_2, NF_ARG_3, or NF_ARG_ANY
            int scc;                       // System code call flag (fast path)
            union {
                obj_t *(*f_onearg)(obj_t *);
                obj_t *(*f_twoarg)(obj_t *, obj_t *);
                obj_t *(*f_threearg)(obj_t *, obj_t *, obj_t *);
                obj_t *(*f_anyarg)(obj_t **, size_t);
            } v;
        } native;

        struct {                           // FUN_CODED
            size_t lp;                     // Entry point (FISH instruction index)
        } coded;
    } v;
} fun_t;
```

### Class Representation (`class_t`)

```c
typedef struct __class_s {
    char    *name;     // Class name
    char   **slots;    // Parallel array: property names
    obj_t  **vals;     // Parallel array: property values
    size_t   svl;      // Current slot count
    size_t   svc;      // Slot capacity
} class_t;
```

Member access (`container_access` in [bytecode.c](bytecode.c)) performs a linear scan of `slots[]` to find the matching name, then returns the corresponding `vals[]` entry.

### Memory Allocation Wrappers

All heap operations go through [malloc.h](malloc.h) / [malloc.c](malloc.c):

| Macro | Underlying |
|---|---|
| `SFMALLOC(size)` | `__sf_malloc` → `malloc` |
| `SFREALLOC(ptr, size)` | `__sf_realloc` → `realloc` |
| `SFFREE(ptr)` | `__sf_free` → `free` |
| `SFSTRDUP(str)` | `__sf_strdup` → `strdup` |

This indirection allows future replacement with a custom allocator (arena, pool, etc.) by modifying only these four functions.

---

## 9. Hash Table Implementation

**Files:** [ht.h](ht.h), [ht.c](ht.c)

The hash table is used during code generation for symbol-to-slot resolution (variable name → slot index mapping).

### Structure

```c
typedef struct {
    htval_t    *vals;       // Main bucket array
    size_t      cap;        // Current capacity (always power of two)
    size_t      entries;    // Active entries
    size_t      tombstones; // Deleted entries (lazy deletion)
    fastcache_t fast;       // Inline cache for small tables
} hashtable_t;
```

### Entry States

| State | Meaning |
|---|---|
| `HT_EMPTY` | Slot has never been used |
| `HT_ACTIVE` | Slot contains a live key/value pair |
| `HT_TOMBSTONE` | Slot was deleted; skip during probing but reusable for insertion |

### Hash Function: FNV-1a

The hash table uses the **FNV-1a** (Fowler–Noll–Vo) algorithm:

```
hash = FNV_OFFSET_BASIS
for each byte in key:
    hash ^= byte
    hash *= FNV_PRIME
return hash
```

FNV-1a provides excellent distribution for short string keys (variable names) with minimal computation.

### Probing Strategy

- **Open addressing** with a hybrid linear/quadratic strategy.
- When the table has fewer than `SF_HT_LINEAR_CUTOFF` (12) entries, linear probing is used.
- For larger tables, quadratic probing avoids clustering.
- The table resizes (doubles capacity) when the load factor exceeds a threshold.

### Fast Cache Optimization

For very small scopes (≤ 8 entries), the `fastcache_t` provides an inline flat array:

```c
typedef struct {
    char  *keys[SF_FASTCACHE_SIZE];   // 8 key slots
    void  *vals[SF_FASTCACHE_SIZE];   // 8 value slots
    size_t c;                          // Entry count
} fastcache_t;
```

Lookups in the fast cache are a simple linear scan of 8 entries — likely to fit in a single cache line. When the fast cache is full, entries spill to the main hash table.

---

## 10. Concurrency Model

**Files:** [mut.h](mut.h), [mut.c](mut.c)

### Current State

Sunflower's VM execution is **single-threaded**. Concurrency primitives exist solely to protect shared mutable state in the object store:

| Primitive | Location | Purpose |
|---|---|---|
| `sfmutex_t` (object store) | `sf_objstore_req()` | Serialize object allocation from the global store |
| `atomic_int ref_count` | `obj_t.meta.ref_count` | Lock-free reference count updates |

### Platform Abstraction

```c
typedef struct {
#if defined(_WIN32)
    HANDLE mut;              // Windows mutex handle
#else
    pthread_mutex_t mut;     // POSIX threads mutex
#endif
} sfmutex_t;
```

### Future Considerations

No scheduler, job queue, or parallel bytecode execution exists. Future work could introduce lightweight green threads or coroutines, but the current design prioritizes simplicity and deterministic execution order.

---

## 11. Code Generation

**Files:** [codegen.h](codegen.h), [codegen.c](codegen.c)

The code generator translates the AST (`StmtSM`) into a stream of FISH instructions written into `vm_t.insts`.

### Symbol Resolution

Variable names are resolved to numbered slots using a stack of hash tables:

```
┌─────────────┐
│  ht[htl-1]  │ ← current scope (innermost)
├─────────────┤
│  ht[htl-2]  │ ← enclosing scope
├─────────────┤
│     ...     │
├─────────────┤
│   ht[0]     │ ← global scope
└─────────────┘
```

- `push_ht()` / `pop_ht()`: manage scope entry/exit.
- `add_var(name)`: allocates a new slot in the current scope, inserting a `vval_t { pos, slot }` into the top hash table.
- `get_var(name)`: searches from top to bottom; returns the slot number and scope type.

### PRESERVE / RESTORE Macros

When entering a nested scope (function body, class body), the codegen saves and restores its metadata:

```c
#define PRESERVE(vm)  \
    int _saved_slot = (vm)->meta.slot; \
    size_t _saved_lslot = (vm)->meta.l_slot; \
    size_t _saved_nslot = (vm)->meta.n_slot;

#define RESTORE(vm)  \
    (vm)->meta.slot = _saved_slot; \
    (vm)->meta.l_slot = _saved_lslot; \
    (vm)->meta.n_slot = _saved_nslot;
```

This ensures that slot counters reset properly when leaving a function or class scope.

### Statement Compilation

| Statement | FISH Output |
|---|---|
| `STMT_VARDECL` | Compile value expression → `OP_STORE` / `OP_STORE_FAST` / `OP_STORE_NAME` |
| `STMT_FUNCALL` | Compile args, compile callee → `OP_CALL` with `b=0` (discard return) |
| `STMT_IFBLOCK` | Compile condition → `OP_JUMP_IF_FALSE` → compile body → `OP_JUMP` (skip else) → patch targets → compile else body |
| `STMT_WHILE` | Record loop start → compile condition → `OP_JUMP_IF_FALSE` → compile body → `OP_JUMP` (back to start) → patch exit |
| `STMT_FUNDECL` | `OP_JUMP` (skip body) → emit body instructions → `OP_RETURN a=0` → `OP_LOAD_FUNC_CODED` → `OP_STORE` |
| `STMT_RETURN` | Compile return expression → `OP_RETURN a=1` |
| `STMT_CLASSDECL` | `OP_LOAD_BUILDCLASS` → compile body (all stores become `OP_STORE_NAME`) → `OP_LOAD_BUILDCLASS_END` → `OP_STORE` |

### Expression Compilation

| Expression | FISH Output |
|---|---|
| `EXPR_CONST` | `OP_LOAD_CONST` (add constant to pool, emit index) |
| `EXPR_VAR` | `OP_LOAD` / `OP_LOAD_FAST` / `OP_LOAD_NAME` (based on resolved scope) |
| `EXPR_ADD_1` | Compile inner expression → `OP_ADD_1` |
| `EXPR_ARITHMETIC` | Emit operands in postfix order: constants as `OP_LOAD_CONST`, variables as loads, operators as `OP_ADD`/`OP_SUB`/`OP_MUL`/`OP_DIV` |
| `EXPR_FUNCALL` | Compile args, compile callee → `OP_CALL` with `b=1` (keep return) |
| `EXPR_CMP` | Compile left, compile right → `OP_CMP a=type` |
| `EXPR_DOT_ACCESS` | Compile left expression → `OP_DOT_ACCESS c=name` |

---

## 12. FISH Bytecode Decomposition Examples

### Example 1: Simple Assignment with Constant Folding

**Source:**
```sf
a = 1 + 2
putln(a)
```

**AST:** `STMT_VARDECL(name=EXPR_VAR("a"), val=EXPR_CONST(3))` — constant folding collapses `1 + 2` to `3` at parse time.

**FISH bytecode:**
```
0000  OP_LOAD_CONST       a=0            ; push const_pool[0] = 3
0001  OP_STORE            a=0            ; pop → globals[0] ("a")
0002  OP_LOAD             a=1            ; push globals[1] (putln)
0003  OP_LOAD             a=0            ; push globals[0] (a = 3)
0004  OP_CALL             a=1  b=0       ; call putln(3), discard return
0005  OP_RETURN           a=0            ; implicit return (push none)
```

**Execution trace:**

| Step | IP | Instruction | Stack | Globals |
|---|---|---|---|---|
| 1 | 0 | `LOAD_CONST 0` | `[3]` | `[—, putln]` |
| 2 | 1 | `STORE 0` | `[]` | `[3, putln]` |
| 3 | 2 | `LOAD 1` | `[putln]` | `[3, putln]` |
| 4 | 3 | `LOAD 0` | `[putln, 3]` | `[3, putln]` |
| 5 | 4 | `CALL 1,0` | `[]` → prints `3` | `[3, putln]` |
| 6 | 5 | `RETURN 0` | — frame ends — | |

---

### Example 2: Recursive Function

**Source:**
```sf
fun factorial(n)
    if n == 0
        return 1
    return n * factorial(n - 1)

putln(factorial(5))
```

**FISH bytecode (conceptual):**
```
0000  OP_JUMP             a=8            ; skip function body
0001  OP_LOAD_FAST        a=0  b=0       ; push local[0] (n)
0002  OP_LOAD_CONST       a=0            ; push 0
0003  OP_CMP              a=0            ; CMP_EQEQ → push bool
0004  OP_JUMP_IF_FALSE    a=7            ; if n != 0, skip to L_else
0005  OP_LOAD_CONST       a=1            ; push 1
0006  OP_RETURN           a=1            ; explicit return 1
0007  OP_LOAD_FAST        a=0  b=0       ; L_else: push n
0008  OP_LOAD_FAST        a=0  b=0       ; push n
0009  OP_LOAD_CONST       a=1            ; push 1
0010  OP_SUB                             ; n - 1
0011  OP_LOAD             a=0            ; push factorial (global)
0012  OP_CALL             a=1  b=1       ; factorial(n-1), keep return
0013  OP_MUL                             ; n * factorial(n-1)
0014  OP_RETURN           a=1            ; explicit return
0015  OP_RETURN           a=0            ; implicit return (safety)
0016  OP_LOAD_FUNC_CODED  a=1  b=1       ; create fun(entry=1, arity=1)
0017  OP_STORE            a=0            ; globals[0] = factorial
0018  OP_LOAD_CONST       a=2            ; push 5
0019  OP_LOAD             a=0            ; push factorial
0020  OP_CALL             a=1  b=1       ; factorial(5), keep return
0021  OP_LOAD             a=1            ; push putln
0022  ... (call putln)                   ; print result
```

**Call stack evolution:**
```
Call: factorial(5)
  ├── Frame 1: n=5, evaluates n*factorial(4)
  │   ├── Frame 2: n=4, evaluates n*factorial(3)
  │   │   ├── Frame 3: n=3, evaluates n*factorial(2)
  │   │   │   ├── Frame 4: n=2, evaluates n*factorial(1)
  │   │   │   │   ├── Frame 5: n=1, evaluates n*factorial(0)
  │   │   │   │   │   └── Frame 6: n=0, returns 1
  │   │   │   │   │   → 1*1 = 1
  │   │   │   │   → 2*1 = 2
  │   │   │   → 3*2 = 6
  │   │   → 4*6 = 24
  │   → 5*24 = 120
  └── Output: 120
```

---

### Example 3: Class Declaration

**Source:**
```sf
class Point
    x = 10
    y = 20

putln(Point.x)
```

**FISH bytecode:**
```
0000  OP_LOAD_BUILDCLASS      a=5        ; enter name frame, END at IP 5
0001  OP_LOAD_CONST           a=0        ; push 10
0002  OP_STORE_NAME           a=0  c="x" ; names[0] = "x", vals[0] = 10
0003  OP_LOAD_CONST           a=1        ; push 20
0004  OP_STORE_NAME           a=1  c="y" ; names[1] = "y", vals[1] = 20
0005  OP_LOAD_BUILDCLASS_END  a=0        ; build class_t{x:10, y:20}, push
0006  OP_STORE                a=0        ; globals[0] = Point
0007  OP_LOAD                 a=1        ; push putln
0008  OP_LOAD                 a=0        ; push Point
0009  OP_DOT_ACCESS           c="x"      ; pop Point, push Point.x (= 10)
0010  OP_CALL                 a=1  b=0   ; call putln(10), discard return
0011  OP_RETURN               a=0        ; implicit return
```

**Output:** `10`

---

### Example 4: While Loop with Increment

**Source:**
```sf
i = 0
while i < 5
    putln(i)
    i = i + 1
```

**FISH bytecode:**
```
0000  OP_LOAD_CONST       a=0            ; push 0
0001  OP_STORE            a=0            ; globals[0] = i
0002  OP_LOAD             a=0            ; L_top: push i
0003  OP_LOAD_CONST       a=1            ; push 5
0004  OP_CMP              a=2            ; CMP_LE (i < 5), push bool
0005  OP_JUMP_IF_FALSE    a=12           ; if false, exit loop
0006  OP_LOAD             a=1            ; push putln
0007  OP_LOAD             a=0            ; push i
0008  OP_CALL             a=1  b=0       ; call putln(i), discard return
0009  OP_LOAD             a=0            ; push i
0010  OP_ADD_1                           ; i + 1 (optimized increment)
0011  OP_STORE            a=0            ; globals[0] = i + 1
0012  OP_JUMP             a=2            ; back to L_top
0013  OP_RETURN           a=0            ; implicit return
```

> **Note:** The pattern `i = i + 1` is recognized by the AST parser as `EXPR_ADD_1`, compiling to a single `OP_ADD_1` instruction instead of `OP_LOAD_CONST 1` + `OP_ADD`.

---

## 13. Performance Considerations

### Dispatch

The VM uses `switch`-based dispatch. While simple and portable, each instruction dispatch involves a branch prediction challenge at the switch. Future work could upgrade to:
- **Computed goto / threaded dispatch** (GCC/Clang `__attribute__((musttail))` or `&&label` extensions) for direct-threaded interpretation.
- **Tail-call dispatch** for even lower overhead.

### Hot Paths

| Path | Optimization |
|---|---|
| Arithmetic (`OP_ADD/SUB/MUL/DIV`) | Currently specialized for integer operands; avoids type dispatch overhead for the common case |
| Increment (`OP_ADD_1`) | Dedicated opcode eliminates constant load + binary add (saves 1 instruction per increment) |
| Constant loading | `sf_objstore_req_forconst` returns cached objects for ints -5..255, `""`, and `none` — zero allocation |
| Symbol resolution (codegen) | Fast cache in hash table handles scopes with ≤ 8 variables in a single cache line |
| Object allocation | Free-list reuse avoids `malloc`/`free` churn for short-lived temporaries |

### Memory Characteristics

| Resource | Initial Capacity | Growth Strategy |
|---|---|---|
| Global slots | 512 | Fixed (assert on overflow) |
| Stack | 128 | Fixed (assert on overflow) |
| Frames | 500 | Fixed (assert on overflow) |
| Frame locals | 64 | Fixed per frame |
| Name slots | 8 | Dynamic growth |
| Hash table | Power-of-2 | Double on load threshold |
| Object store | Dynamic | Grows by chunks |
| Instruction array | Dynamic | `vm->inst_cap` doubles |
| Constant pool | Dynamic | `vm->s_mc` doubles |

### Constant Folding

Arithmetic expressions where all operands are compile-time constants are evaluated during parsing:
- `1 + 2 * 3` → `7` (single `OP_LOAD_CONST`)
- `x + 1` → `OP_ADD_1` (single instruction instead of load-const + add)

This reduces bytecode size and eliminates runtime arithmetic for compile-time-known values.

---

## 14. Design Tradeoffs

### Stack VM vs. Register VM

**Chosen: Stack VM.** Operands flow through a value stack, making code generation from tree-structured ASTs trivial: emit left, emit right, emit operation. A register VM could reduce push/pop overhead but would significantly complicate the compiler with register allocation, spilling, and live range analysis.

### Reference Counting vs. Tracing GC

**Chosen: Reference counting.** Provides deterministic destruction with no stop-the-world pauses. The cost is atomic operations on every reference change and the inability to collect cycles. Since Sunflower currently lacks closures and circular data structures, cycles are not a practical concern.

### Flat Object Store vs. Type-Segregated Pools

**Chosen: Flat store.** All `obj_t` instances live in a single array regardless of type. This simplifies the allocator and free-list but may waste memory when objects have varying lifetimes. Type-segregated pools could improve cache locality for specific operations.

### FNV-1a vs. More Complex Hash Functions

**Chosen: FNV-1a.** Excellent distribution for short keys (variable names are typically short) with minimal code. More complex functions (SipHash, xxHash) would improve collision resistance for adversarial inputs but are unnecessary for compiler-internal symbol tables.

### Fixed-Format Instructions vs. Variable-Length

**Chosen: Fixed 4-field `instr_t`.** Every instruction is `{ op, a, b, c }` regardless of how many operands are used. This wastes space for zero-operand instructions (like `OP_ADD`) but eliminates the need for variable-length decoding, simplifying the dispatch loop and enabling random access into the instruction stream.

### Current Limitations

| Limitation | Impact | Mitigation Path |
|---|---|---|
| No closures/upvalues | Cannot capture variables from enclosing scopes after function returns | Implement upvalue cells with copy-on-capture |
| Integer-specialized arithmetic | Float and mixed-type arithmetic not fully supported in VM | Add type dispatch or tagged arithmetic opcodes |
| No method dispatch | Classes are property bags only | Add `OP_CALL_METHOD` opcode and vtable/slot caching |
| Fixed stack/frame limits | Deep recursion will assert | Dynamic growth with configurable limits |
| No debug info | Cannot map bytecode back to source lines | Add source-map table alongside instruction stream |
| No bytecode serialization | Cannot save/load compiled FISH bytecode | Define binary format for `.fishc` files |

---

## 15. Appendix: Constants & Limits

### Compile-Time Constants

| Constant | Value | Location | Purpose |
|---|---|---|---|
| `SF_VM_GLOBALS_CAP` | 512 | [bytecode.h](bytecode.h) | Maximum global variable slots |
| `SF_VM_STACK_CAP` | 128 | [bytecode.h](bytecode.h) | Maximum operand stack depth |
| `SF_VM_FRAME_CAP` | 500 | [bytecode.h](bytecode.h) | Maximum call frame depth |
| `SF_FRAME_LOCALS_CAP` | 64 | [bytecode.h](bytecode.h) | Local variables per frame |
| `SF_VM_HT_CAP` | 8 | [bytecode.h](bytecode.h) | Initial hash table stack capacity |
| `SF_VM_NAME_CAP` | 8 | [bytecode.h](bytecode.h) | Initial name-scope capacity |
| `SF_FASTCACHE_SIZE` | 8 | [ht.h](ht.h) | Fast cache inline entries |
| `SF_HT_LINEAR_CUTOFF` | 12 | [ht.h](ht.h) | Linear probing threshold |
| `SF_TOKEN_STATEM_VALS_CAP` | 64 | [token.h](token.h) | Initial token array capacity |
| `STMT_SM_VALS_CAP` | 512 | [stmt.h](stmt.h) | Initial statement array capacity |

### Slot Types

| Constant | Value | Scope |
|---|---|---|
| `SF_VM_SLOT_GLOBAL` | 0 | Top-level variables |
| `SF_VM_SLOT_LOCAL` | 1 | Function-local variables |
| `SF_VM_SLOT_NAME` | 2 | Class construction slots |

### Comparison Encodings

| `CmpType` | Value | Operator |
|---|---|---|
| `CMP_EQEQ` | 0 | `==` |
| `CMP_NEQ` | 1 | `!=` |
| `CMP_LE` | 2 | `<` |
| `CMP_GE` | 3 | `>` |
| `CMP_LEQ` | 4 | `<=` |
| `CMP_GEQ` | 5 | `>=` |
