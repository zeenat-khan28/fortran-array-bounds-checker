# Design

## Problem

Fortran is widely used in scientific computing — weather simulation, aerospace, particle physics — and has been for decades. It allows array declarations with arbitrary lower and upper bounds:

```fortran
REAL :: A(-5:10)     ! lower bound -5, upper bound 10
REAL :: B(100:200)   ! starts at 100
REAL :: C(1:10)      ! default style
```

Without bounds checking, an out-of-bounds access either:
1. Causes a **segfault** at runtime (best case — at least you know something went wrong), or
2. **Silently corrupts** another variable's memory — the program keeps running with wrong data, with no indication anything failed.

In safety-critical domains, option 2 is dangerous. The goal of this project is to move that detection from runtime to **compile time**.

---

## Approach

We implemented a **custom semantic analysis pass** inside Flang's existing compiler pipeline.

### Why semantic analysis?

The compiler pipeline is:

```
Source → Parser → Name Resolution → Semantic Analysis → Code Generation → Binary
```

Semantic analysis is the right stage because:
- Parsing is already done (we have a structured parse tree, not raw text)
- Name resolution is complete (every array name maps to its declared symbol)
- PARAMETER constants have been substituted into expressions
- No code has been generated yet — we can still abort cleanly

This means we can evaluate constant subscripts, look up declared bounds from the symbol table, and emit errors before any executable is produced.

### How the pass works

1. **Declaration phase** — intercept every `EntityDecl` node. If it's an array, read and store its declared bounds per dimension.
2. **Access phase** — intercept every `ArrayElement` node. Look up the stored bounds, attempt to evaluate the subscript via constant folding, and compare.
3. **Reporting** — emit a structured error with the array name, dimension, index, declared range, and a nearest-valid-index hint. Print a summary at the end.

The pass uses Flang's `BaseChecker` infrastructure and is registered into the `SemanticsVisitor` template, so it runs automatically on every file compiled with the modified binary.

---

## Alternatives Considered

### Runtime bounds checking (`-fbounds-check`)

GFortran and some Flang builds support a flag that inserts bounds checks into the compiled binary. This catches all violations — including ones with runtime-variable indices — but:
- It requires re-compiling the program with the flag
- It adds runtime overhead (unacceptable in production HPC code)
- The program still has to actually execute the bad path before the error surfaces
- It doesn't help with legacy code that can't be rerun

Our approach catches a subset of violations (those with statically evaluable indices) but does so with **zero runtime cost** and **before deployment**.

### A standalone source-level linter

An external tool that parses Fortran source without going through the compiler would avoid the LLVM build complexity, but:
- It would need to duplicate name resolution and constant folding logic that Flang already provides
- It would not have access to fully resolved PARAMETER values and type information
- Integrating into the compiler means it runs automatically — no separate tool invocation required

We chose the compiler-integrated approach for correctness and automation.

### GCC / GFortran

GFortran doesn't expose an API for adding custom semantic passes. Flang, being built on LLVM's modular architecture, provides `BaseChecker`, the `SemanticsVisitor` template, and the `evaluate` library — everything needed to write and register a new pass cleanly.
