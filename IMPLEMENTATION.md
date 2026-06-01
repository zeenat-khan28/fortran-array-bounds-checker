# Implementation

## Files Added

### `src/check-array-bounds.h`
→ Copied to `flang/include/flang/Semantics/check-array-bounds.h`

Declares:
- `DimBounds` — struct holding lower bound, upper bound, and dimension number for one array dimension. Uses `std::optional` for bounds (assumed-size arrays have no upper bound).
- `BoundsCheckStats` — counters for verified, unverifiable, assumed-size, common-block, allocatable, and error accesses.
- `ArrayBoundsChecker` — the main checker class, inheriting from `BaseChecker`.

### `src/check-array-bounds.cpp`
→ Copied to `flang/lib/Semantics/check-array-bounds.cpp`

Contains the full checker logic (described below).

---

## Files Modified

### `flang/lib/Semantics/CMakeLists.txt`
Added `check-array-bounds.cpp` to the compiled file list so Ninja includes it in the build:

```bash
sed -i '17a\  check-array-bounds.cpp' flang/lib/Semantics/CMakeLists.txt
```

### `flang/lib/Semantics/semantics.cpp`
Two changes:

1. Added the include:
```cpp
#include "flang/Semantics/check-array-bounds.h"
```

2. Added `ArrayBoundsChecker` to the `SemanticsVisitor` template list so it runs on every compilation:
```cpp
// Before:
using StatementSemanticsPass2 = SemanticsVisitor<..., StopChecker>;
// After:
using StatementSemanticsPass2 = SemanticsVisitor<..., StopChecker, ArrayBoundsChecker>;
```

---

## LLVM / Flang APIs Used

| API | Purpose |
|-----|---------|
| `BaseChecker` | Base class for all Flang semantic passes; provides `Enter`/`Leave` visitor hooks |
| `SemanticsContext` | Passed to the checker; used to emit diagnostics via `context_.Say()` |
| `evaluate::ToInt64` | Constant-folds an expression to a 64-bit integer; handles PARAMETER substitution automatically |
| `ObjectEntityDetails` | Symbol detail type for variables; provides `.shape()` for array dimensions |
| `ShapeSpec::lbound()` / `.ubound()` | Returns the declared lower/upper bound of one array dimension |
| `GetExplicit()` | Extracts the explicit bound expression from a `ShapeSpec` (returns `std::optional`) |
| `parser::ArrayElement` | Parse tree node for array subscript expressions like `A(5)` or `GRID(3,4)` |
| `parser::EntityDecl` | Parse tree node for variable declarations like `REAL :: A(1:10)` |
| `parser::SectionSubscript` | Variant type holding either a scalar subscript (`IntExpr`) or a slice (`SubscriptTriplet`) |
| `llvm::errs()` | Used to write hint messages directly to stderr, bypassing Flang's diagnostic suppression |

---

## Checker Algorithm

### Step 1 — Collect bounds from declarations

`Enter(const parser::EntityDecl &)` is called for every variable declaration.

```
for each declaration:
    resolve the symbol from the parse tree
    if symbol has ObjectEntityDetails and non-empty shape:
        for each dimension in shape:
            call evaluate::ToInt64 on lbound expression → lower
            call evaluate::ToInt64 on ubound expression → upper
            if ubound is absent (A(*)):
                mark as assumed-size
            store DimBounds{lower, upper, dimIndex} in arrayBoundsMap_[name]
        store rank in arrayRankMap_[name]
```

PARAMETER constants are resolved transparently by `evaluate::ToInt64` — no special handling needed.

### Step 2 — Check each access

`Enter(const parser::ArrayElement &)` is called for every array subscript expression.

```
for each array access A(i, j, ...):
    look up name in arrayBoundsMap_
    if not found: return (no bounds known)

    for each subscript s at dimension d:
        if s is a SubscriptTriplet (slice A(lo:hi)):
            attempt ToInt64 on slice_lo → compare against declared lower
            attempt ToInt64 on slice_hi → compare against declared upper
        else if s is an IntExpr (scalar):
            attempt ToInt64 on s → index
            if folding fails: emit unverifiable warning, continue
            if index < declared_lower or index > declared_upper:
                call CheckOneDimension → emit error + hint
            else:
                increment verified counter
```

### Step 3 — Emit diagnostics

`CheckOneDimension` formats the error message. For multi-dimensional arrays (rank > 1) it includes "dimension X of Y":

```
error: Array 'grid' dimension 1 of 2: index 4 is outside declared bounds [1:3]
  hint: nearest valid index for 'grid' dimension 1 is 3 (valid range is [1:3])
```

The hint is written via `llvm::errs()` rather than `context_.Say()` to avoid Flang's message suppression after repeated errors.

### Step 4 — Summary

The checker destructor prints a summary of all counters:

```
===== Array Bounds Check Summary =====
  Verified safe accesses : 22
  Unverifiable (warnings): 0
  Assumed-size accesses  : 0
  Common block arrays    : 0
  Allocatable tracked    : 2
  Out-of-bounds (errors) : 4
======================================
```

---

## Known Limitation

Loop variable analysis was attempted but removed. Adding an `Enter` method for `DoConstruct` nodes caused an ambiguity error because `DoForallChecker` (already in the visitor list) also handles `DoConstruct`. C++ cannot resolve the call when two checkers in the same composite visitor handle the same node type. As a result, loop variable accesses are reported as **unverifiable warnings** rather than being analysed against DO bounds. This is conservative — no false positives.
