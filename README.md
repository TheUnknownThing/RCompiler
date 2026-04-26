# RCompiler

A C++ implementation of a compiler for a subset of the Rust programming language.

## Project Structure

```
RCompiler/
├── src/
│   ├── ast/
│   ├── backend/
│   ├── ir/
│   ├── lexer/
│   ├── opt/
│   ├── preprocessor/
│   ├── semantic/
│   ├── utils/
│   └── main.cpp
├── include/
│   ├── ast/
│   ├── backend/
│   ├── common/
│   ├── ir/
│   ├── lexer/
│   ├── opt/
│   ├── preprocessor/
│   ├── semantic/
│   └── utils/
├── tests/
│   └── ci_*.cpp
├── CMakeLists.txt
├── README.md
└── whitelist.md
```

## Original Repo

This project implements [RCompiler-Spec](https://github.com/peterzheng98/RCompiler-Spec/) as homework of [Compilers 2025](https://ipads.se.sjtu.edu.cn/courses/compilers/index.shtml) at Shanghai Jiao Tong University.

## Implemented Features

See [whitelist.md](whitelist.md) for the concrete, implementable subset of the Rust language that the RCompiler parser and type checker will support. This subset is derived from the current Rust specification, excluding features that are explicitly removed or marked as undefined behavior.

## Implementation Status

- [x] Preprocessor
    - Remove Comments
    - Check invalid ASCII character

- [x] Lexer
    - Identify keywords / identifiers
    - Classify literals (strings, numbers, etc.)

- [x] Parser
    - Parse expr, stmts, items

- [x] Semantic Checking
    - [x] First Pass
        - Symbol Table Construction
        - Scope Management
    - [x] Builtin Types and Functions
    - [x] Second Pass
        - Type Resolution
        - Function Signature Verification
        - Constant Evaluation
    - [x] Third Pass
        - Promote `impl` blocks to `struct`/`trait` definitions
    - [x] Fourth Pass
        - Pattern Binding
        - Type Checking
        - Borrow Checking
        - Expression Analysis
    - [x] Control Flow Analysis
        - Return Path Verification

- [x] IR Codegen
    - [x] LLVM IR Emission
    - [x] Module / function / basic block IR construction

- [x] IR Optimization
    - [x] CFG construction and refresh
    - [x] Aggressive Dead Code Elimination (DCE)
        - Remove instructions after terminators
        - Fold constant conditional branches
        - Eliminate unreachable blocks
        - Remove dead instructions (those with no side effects and whose results are not used)
    - [x] Mem2Reg / SSA promotion
    - [x] Function inlining
    - [x] Instruction combining
        - Constant folding
        - Algebraic simplification
        - Strength reduction
    - [x] Sparse Conditional Constant Propagation (SCCP)
    - [x] CFG simplification
        - Remove unreachable blocks
        - Fold trivial phi nodes
        - Merge trivial basic blocks
    - [ ] GVN & GCM (TBD)

- [x] Backend Codegen