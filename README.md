<div align="center">

# 🌻 Sunflower

### A lightweight, dynamically typed programming language powered by the **FISH** bytecode VM

*Flower Instruction Set Hybrid: a clean, stack-based instruction set architecture written in C23*

[![C Standard](https://img.shields.io/badge/C-C23-blue.svg)](#build-instructions)
[![Build System](https://img.shields.io/badge/build-CMake%20≥%203.10-064F8C.svg)](#build-instructions)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](#license)

---

</div>

## Table of Contents

- [Overview](#overview)
- [FISH — Flower Instruction Set Hybrid](#fish--flower-instruction-set-hybrid)
- [Language Features](#language-features)
- [Syntax Examples](#syntax-examples)
- [Build Instructions](#build-instructions)
- [Running the Interpreter](#running-the-interpreter)
- [Testing](#testing)
- [Project Structure](#project-structure)
- [FISH Bytecode Example](#fish-bytecode-example)
- [Embedding Sunflower](#embedding-sunflower)
- [Contribution Guidelines](#contribution-guidelines)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview

Sunflower is a small, dynamically typed programming language implemented entirely in **C23**. It ships a complete toolchain from source to execution: lexer, indentation-sensitive parser, AST builder, bytecode compiler, and a stack-based virtual machine executing the **FISH** (_Flower Instruction Set Hybrid_) instruction set.

The runtime favors simplicity and predictability:

- **Deterministic memory management** via atomic reference counting — no stop-the-world GC pauses.
- **Readable bytecode** with a compact 4-field instruction format (`opcode`, `a`, `b`, `c`).
- **Dual execution paths** — an AST tree-walking interpreter for rapid prototyping and a compiled FISH bytecode path for performance.
- **Embeddable by design** — link against the `sunflower` library, register native C functions, and run scripts programmatically.
- **Cross-platform** — builds on macOS, Linux, and Windows with pthread/Win32 threading abstraction.

### Design Philosophy

| Principle | Implementation |
|---|---|
| Small surface area | 21 opcodes, 7 expression types, 8 statement types |
| Explicit over implicit | No hidden allocations; users see exactly what's on the stack |
| Readable bytecode | Every FISH instruction has a clear, documented purpose |
| Embeddable | Clean C API with `SF_API` export macros; C++ compatible headers |
| Fast enough | Switch-dispatch VM with cached constants and free-list object reuse |

### What Sunflower Is Not (Yet)

Sunflower is a young language. It does not yet have closures/upvalues, a REPL, a full standard library, module imports, or parallel bytecode execution. These are on the [roadmap](#roadmap).

---

## FISH — Flower Instruction Set Hybrid

At the heart of Sunflower is **FISH** (_Flower Instruction Set Hybrid_), a purpose-built bytecode instruction set designed for clarity, compactness, and easy compilation.

### Why FISH?

FISH was designed from the ground up as a minimal but complete instruction set for a dynamically typed language. Rather than borrowing an existing bytecode format, Sunflower defines its own to keep the compiler-to-VM contract transparent and hackable.

### Key Characteristics

| Feature | Detail |
|---|---|
| **Architecture** | Stack-based — operands pushed and popped from a value stack |
| **Instruction format** | Fixed 4-field: `{ opcode_t op; int a; int b; char *c; }` |
| **Instruction count** | 21 opcodes covering loads, stores, arithmetic, control flow, function calls, comparisons, class building, and member access |
| **Constant pool** | Indexed array of `const_t` values (int, float, string, bool, none) referenced by `OP_LOAD_CONST` |
| **Three storage scopes** | Global slots, frame-local slots, and name-scope slots (for class construction) |
| **Optimizations** | Dedicated `OP_ADD_1` for increment patterns; AST-level constant folding before codegen |
| **Native interop** | `OP_CALL` dispatches to native C functions with zero-copy argument passing and optional `scc` (system code call) fast path |

### FISH Opcode Summary

| Opcode | Operands | Description |
|---|---|---|
| `OP_LOAD_CONST` | `a` = const index | Push constant from pool onto stack |
| `OP_LOAD` | `a` = global slot | Push global variable onto stack |
| `OP_LOAD_FAST` | `a` = local slot, `b` = depth | Push local variable (supports lexical depth traversal) |
| `OP_LOAD_NAME` | `a` = name slot | Push name-scope variable (class construction) |
| `OP_STORE` | `a` = global slot | Pop stack → store into global slot |
| `OP_STORE_FAST` | `a` = local slot | Pop stack → store into frame-local slot |
| `OP_STORE_NAME` | `a` = name slot, `c` = name | Pop stack → store into name-scope slot |
| `OP_CALL` | `a` = arg count, `b` = keep return | Pop callee + args, invoke; if `b=1` push return value |
| `OP_ADD_1` | — | Pop one int, push (value + 1) — optimized increment |
| `OP_ADD` | — | Pop two values, push their sum |
| `OP_SUB` | — | Pop two values, push difference |
| `OP_MUL` | — | Pop two values, push product |
| `OP_DIV` | — | Pop two values, push quotient |
| `OP_CMP` | `a` = comparison type | Pop two values, push boolean result |
| `OP_JUMP` | `a` = target IP | Unconditional jump |
| `OP_JUMP_IF_FALSE` | `a` = target IP | Pop condition; jump if falsey |
| `OP_LOAD_FUNC_CODED` | `a` = entry IP, `b` = arity | Create coded-function object and push |
| `OP_LOAD_BUILDCLASS` | `a` = end IP | Enter name frame for class construction |
| `OP_LOAD_BUILDCLASS_END` | `a` = start IP | Exit name frame, materialize class, push |
| `OP_DOT_ACCESS` | `c` = member name | Pop container, push named member |
| `OP_RETURN` | `a` = explicit? | Terminate frame; `a=1` user return, `a=0` pushes none |

See [ARCHITECTURE.md](ARCHITECTURE.md) for the complete FISH specification, stack effects, and decomposition examples.

---

## Language Features

### Value Types

| Type | Examples | Internal Representation |
|---|---|---|
| Integer | `42`, `-5`, `0` | `const_t` → `CONST_INT` (C `int`) |
| Float | `3.14`, `0.5` | `const_t` → `CONST_FLOAT` (C `float`) |
| String | `"hello"`, `"world\n"` | `const_t` → `CONST_STRING` (heap `char*`) |
| Boolean | `True`, `False` | `const_t` → `CONST_BOOL` (C `int`) |
| None | `None` | `const_t` → `CONST_NONE` |
| Function | `fun f(x) ...` | `obj_t` → `OBJ_FUNC` → `fun_t` (native or coded) |
| Class | `class Foo ...` | `obj_t` → `OBJ_CLASS` → `class_t` (slot/value arrays) |

### Variables & Scoping

- **Global variables**: stored in a pre-allocated global slot array (`globals[512]`).
- **Local variables**: stored per call frame (`FRAME_LOCAL`) with support for lexical depth lookup across parent frames.
- **Name-scope variables**: used exclusively during class body construction (`FRAME_NAME`).

### Functions

- **Coded functions**: defined in Sunflower source, compiled to FISH bytecode, invoked via `OP_CALL` which pushes a new frame.
- **Native functions**: written in C, registered at runtime with 1-arg, 2-arg, 3-arg, or variadic signatures. Support an `scc` (system code call) flag for fast dispatch.
- Fixed arity enforced at call sites.

### Control Flow

- `if` / `else` conditional branching (compiled to `OP_CMP` + `OP_JUMP_IF_FALSE`)
- `while` loops (compiled to `OP_JUMP` + `OP_JUMP_IF_FALSE` back-edge)
- `return` statements (early exit from function frames)
- Planned: `for`, `break`, `continue`, `try/catch` (keywords reserved)

### Classes

- Property-bag style: classes are declared with slot/value pairs.
- Dot-access member resolution at runtime (`OP_DOT_ACCESS`).
- Currently no methods or inheritance (planned).

### Arithmetic & Comparisons

- Binary operators: `+` `-` `*` `/`
- Unary increment: dedicated `OP_ADD_1` for `x + 1` patterns
- Comparison operators: `==` `!=` `<` `>` `<=` `>=`
- Arithmetic is parsed via shunting-yard algorithm into postfix notation, with constant folding at the AST level.

### Reserved Keywords

```
if  else  for  while  in  to  step  class  fun  return
or  and  not  repeat  import  as  try  catch  break
continue  extends
```

---

## Syntax Examples

### Variables and Arithmetic

```sf
a = 10
b = 20
c = a + b * 2
putln(c)
```

### Functions

```sf
fun greet(name)
    put("Hello, ")
    putln(name)

greet("Sunflower")
```

### Recursive Functions

```sf
fun factorial(n)
    if n == 0
        return 1
    return n * factorial(n - 1)

putln(factorial(5))
```

### Control Flow

```sf
x = 10
if x > 5
    putln("big")
else
    putln("small")
```

### While Loops

```sf
i = 0
while i < 5
    putln(i)
    i = i + 1
```

### Classes

```sf
class Point
    x = 0
    y = 0

class Origin
    x = 100
    y = 200

putln(Origin.x)
putln(Origin.y)
```

### Nested Conditionals

```sf
a = 10
if a > 5
    if a > 8
        putln("greater than 8")
    else
        putln("between 5 and 8")
else
    putln("5 or less")
```

---

## Build Instructions

### Prerequisites

- **CMake** ≥ 3.10
- A **C23**-compatible compiler (GCC 13+, Clang 16+, or MSVC with `/std:clatest`)
- **pthreads** (Unix) or **Windows API** (Win32) for mutex support

### Release Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Debug Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Clean Rebuild

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Running the Interpreter

The repository builds a static library **`sunflower`** and a test harness **`TEST_EXE`** (see [test/test.c](test/test.c)). The harness demonstrates both execution paths:

| Path | Function | Description |
|---|---|---|
| AST interpreter | `test1()` | Lexes, parses, and tree-walks the AST directly |
| FISH bytecode VM | `test3()` | Lexes, parses, generates FISH bytecode via codegen, then executes on the VM |

```bash
# Run the test harness
./build/test/TEST_EXE
```

The harness loads `test/test.sf` by default. To run a different script, modify the file path in [test/test.c](test/test.c) or embed Sunflower in your own application (see [Embedding Sunflower](#embedding-sunflower)).

---

## Testing

Sunflower uses CTest for automated testing.

```bash
# Run all tests
ctest --test-dir build

# Run with verbose output
ctest --test-dir build --verbose
```

### Test Scripts

| File | Purpose |
|---|---|
| [test/test.sf](test/test.sf) | Class declaration with properties and dot-access |
| [test/ifbranch.sf](test/ifbranch.sf) | Deeply nested if/else branches for conditional compilation testing |

### Test Harness

[test/test.c](test/test.c) contains three test functions:

- **`test1`** — End-to-end AST interpreter path: tokenize → parse → interpret
- **`test2`** — Manual AST construction and interpretation (unit-style)
- **`test3`** — Full FISH pipeline: tokenize → parse → codegen → VM execution

Each test registers native functions (`putln`, `put`, `write`) and validates output.

---

## Project Structure

```
sfoptvm/
├── CMakeLists.txt          # Top-level build configuration
├── README.md               # This file
├── ARCHITECTURE.md         # Deep technical architecture document
│
├── header.h                # Universal includes, platform detection, SF_API macro
├── malloc.h / malloc.c     # Memory allocation wrappers (SFMALLOC, SFFREE, SFSTRDUP)
├── mut.h / mut.c           # Cross-platform mutex (pthread / Win32 HANDLE)
│
├── token.h / token.c       # Lexer — source text → token stream (TokenSM)
├── ast.h / ast.c           # Indentation-sensitive parser → AST (StmtSM)
├── expr.h / expr.c         # Expression types and utilities (expr_t, 7 variants)
├── stmt.h / stmt.c         # Statement types (stmt_t, 8 variants including STMT_EOF)
├── const.h / const.c       # Constant value representation (const_t — int/float/string/bool/none)
├── arith.h / arith.c       # Shunting-yard → postfix, constant folding, arithmetic eval
│
├── codegen.h / codegen.c   # FISH bytecode compiler (AST → instr_t stream)
├── bytecode.h / bytecode.c # FISH VM — instruction types, VM state, execution loop
│
├── object.h / object.c     # Object system — tagged unions, refcounting, object store
├── fun.h / fun.c           # Function representation (native / coded)
├── cl.h / cl.c             # Class representation (slot/value arrays)
├── ht.h / ht.c             # Hash table — FNV-1a, open addressing, fastcache
│
├── mod.h / mod.c           # Module representation (file-based)
├── parser.h / parser.c     # AST tree-walking interpreter (alternative execution path)
├── sunflower.h / sunflower.c # Top-level API entry points
│
├── bench.py                # Benchmarking script
└── test/
    ├── CMakeLists.txt      # Test build configuration
    ├── test.c              # Test harness (AST + FISH VM paths)
    ├── test.sf             # Class/property test script
    └── ifbranch.sf         # Nested conditional test script
```

---

## FISH Bytecode Example

### Source

```sf
a = 1 + 2
putln(a)
```

### Compilation & Execution

**Step 1 — Constant Folding.** The AST parser recognizes `1 + 2` as a constant arithmetic expression and folds it to `3` at compile time.

**Step 2 — FISH Codegen.** The bytecode compiler emits:

```
Addr  Opcode              Operands       Description
────  ──────────────────  ─────────────  ──────────────────────────────────
0000  OP_LOAD_CONST       a=0            Push const_pool[0] (= 3) onto stack
0001  OP_STORE            a=0            Pop → globals[0] (variable "a")
0002  OP_LOAD             a=1            Push globals[1] (function "putln")
0003  OP_LOAD             a=0            Push globals[0] (variable "a")
0004  OP_CALL             a=1  b=0       Call putln with 1 arg; discard return
0005  OP_RETURN           a=0            Implicit module return (push none)
```

**Step 3 — VM Execution Trace.**

| IP | Stack (before) | Operation | Stack (after) |
|---|---|---|---|
| 0 | `[]` | Push `3` | `[3]` |
| 1 | `[3]` | Pop → `globals[0]` | `[]` |
| 2 | `[]` | Push `putln` | `[putln]` |
| 3 | `[putln]` | Push `globals[0]` = `3` | `[putln, 3]` |
| 4 | `[putln, 3]` | Call `putln(3)` → prints `3` | `[]` |
| 5 | `[]` | Return `none` | — frame ends — |

**Output:** `3`

---

## Embedding Sunflower

Sunflower is designed to be embedded in C/C++ applications. Link against the `sunflower` static library and use the pipeline API:

```c
#include "sunflower.h"
#include "token.h"
#include "ast.h"
#include "codegen.h"
#include "bytecode.h"
#include "object.h"

// 1. Initialize the object store (cached constants, free list)
sf_objstore_init();

// 2. Tokenize source
char *source = "putln(42)";
TokenSM *tokens = sf_statem_token_new(source);
sf_token_gen(tokens);

// 3. Parse into AST
StmtSM *ast = /* parse tokens into statement machine */;

// 4. Compile to FISH bytecode
vm_t vm = sf_vm_new();
sf_vm_gen_bytecode(&vm, ast);

// 5. Register native functions
obj_t *putln_obj = sf_objstore_req();
putln_obj->type = OBJ_FUNC;
// ... configure native function ...

// 6. Execute
sf_vm_exec_frame_top(&vm);
```

See [test/test.c](test/test.c) for a complete working example.

---

## Contribution Guidelines

1. **Small, focused PRs** — each PR should address a single concern.
2. **Keep headers C/C++ friendly** — all headers use `extern "C"` guards.
3. **Add tests** — any change to semantics, opcodes, or the FISH instruction set must include test coverage under `test/`.
4. **Build and test before submitting:**
   ```bash
   cmake --build build && ctest --test-dir build
   ```
5. **Document changes** — new FISH opcodes or VM behaviors must be documented in [ARCHITECTURE.md](ARCHITECTURE.md).
6. **Follow existing style** — `snake_case` for functions, `sf_` prefix for public API, `SFMALLOC`/`SFFREE` for memory.

---

## Roadmap

| Priority | Feature | Status |
|---|---|---|
| 🔴 High | REPL and incremental compilation | Planned |
| 🔴 High | Closures and upvalue capture | Planned |
| 🟡 Medium | Standard library (I/O, collections, math) | Planned |
| 🟡 Medium | Module import system | Keywords reserved |
| 🟡 Medium | Methods and inheritance for classes | Planned |
| 🟢 Future | Threaded/computed-goto dispatch for FISH | Planned |
| 🟢 Future | Inline caching and peephole optimization | Planned |
| 🟢 Future | Optional JIT tier | Planned |
| 🟢 Future | Debug info, source maps, bytecode verifier | Planned |
| 🟢 Future | Exception handling (`try/catch`) | Keywords reserved |
| 🟢 Future | `for` loops, `break`, `continue` | Keywords reserved |

---

## License

MIT License.
