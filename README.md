# RCompiler

A C++ implementation of a compiler for a subset of the Rust programming language.

## Project Structure

```
RCompiler/
├── src/
│   ├── main.cpp
├── include/
│   ├── lexer/
│   ├── parser/
│   ├── semantic/
│   └── utils/
├── tests/
│   ├── unit/
│   └── integration/
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
    - [x] Dirty Workarounds
        - Handling `exit(0)` in `main`

- [ ] IR Codegen

## Continuous Integration

This repository provides dedicated GitHub Actions workflows to protect each
stage of the frontend pipeline:

- **Preprocessor CI** (`.github/workflows/preprocessor-ci.yml`) builds the
    project and runs the labelled `preprocessor` tests that exercise the fixtures
    in `ci/files/preprocessor` and their expectations in
    `ci/expected_output/preprocessor`.
- **Lexer CI** (`.github/workflows/lexer-ci.yml`) validates tokenization logic
    using the cases in `ci/files/lexer` alongside the expected token streams in
    `ci/expected_output/lexer`.
- **Parser CI** (`.github/workflows/parser-ci.yml`) ensures the parser can
    distinguish the valid and invalid Rust subsets defined under
    `ci/files/parser`.

Each workflow reuses the shared
`Component Test Template` (`.github/workflows/component-template.yml`) to
configure CMake with both `g++` and `clang++`, build only the relevant
`ci_<component>_tests` target, and execute `ctest` for the matching label. The
jobs publish diagnostic logs automatically when a failure occurs. All workflows
support manual triggers via the **Run workflow** button and participate in the
standard `push`/`pull_request` checks.