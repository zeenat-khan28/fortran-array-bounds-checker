# Fortran Array Bounds Checker

A custom semantic analysis pass for **Flang** (the LLVM Fortran compiler) that detects array out-of-bounds accesses at **compile time** — before the program ever runs.

---

## What It Does

Fortran does not check array bounds by default. An access like `A(99)` on an array declared as `REAL :: A(1:10)` compiles silently and either crashes at runtime or — worse — corrupts memory without any error.

This checker integrates directly into Flang's semantic analysis stage and catches violations **during compilation**, including:

- Constant index out-of-bounds (e.g. `A(0)`, `A(11)` on a `1:10` array)
- Non-standard lower bounds (e.g. `B(-6)` on `REAL :: B(-5:5)`)
- Named constant (`PARAMETER`) bounds — e.g. `REAL :: GRID(ROWS, COLS)`
- Multi-dimensional arrays with per-dimension diagnostics
- Array slice bounds (e.g. `A(0:5)` on a `1:10` array)
- Assumed-size arrays (`A(*)`) — warned as unverifiable
- Allocatable arrays tracked from `ALLOCATE` statements
- Common block arrays
- Function argument size mismatches

Runtime-variable indices (loop counters, user input) are flagged as **warnings** — the checker never produces false errors.

---

## How to Build

### Prerequisites

- WSL (Ubuntu 24) or native Linux
- ~50 GB disk space
- 16 GB RAM (build uses `j4` to stay within limits)

```bash
# Install dependencies
sudo apt install -y cmake ninja-build git clang lld python3 \
    build-essential libssl-dev zlib1g-dev libtinfo-dev

# Clone LLVM
git clone --depth=1 https://github.com/llvm/llvm-project.git

# Copy checker source files
cp src/check-array-bounds.h llvm-project/flang/include/flang/Semantics/
cp src/check-array-bounds.cpp llvm-project/flang/lib/Semantics/

# Run the build script
bash build.sh
```

See `build.sh` for the full CMake configuration and registration steps.

---

## How to Run

```bash
# Run all test cases
bash run.sh

# Run a single file
./llvm-project/build/bin/flang-new -fc1 -fsyntax-only testcases/test1.f90
```

### Example Output

```
error: Array 'a' dimension 1: index 11 is outside declared bounds [1:10]
  A(11) = 2.0
    ^^
  hint: nearest valid index for 'a' dimension 1 is 10 (valid range is [1:10])

===== Array Bounds Check Summary =====
  Verified safe accesses : 3
  Unverifiable (warnings): 0
  Out-of-bounds (errors) : 2
======================================
```

---

## Authors

- Zeenat Khan (1RV23CS302)
- Yashaswini K C (1RV23CS298)
- Yug Jain (1RV23CS300)
