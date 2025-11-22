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

## IR Testbench

The IR-1 testcases under `testcases/IR-1` can now be exercised end-to-end with
`scripts/run_ir_tests.py`. The runner automates the workflow that the original
`test_llvm.bash` script documents:

1. Invoke `build/rcompiler` on each `.rx` program referenced by
   `testcases/IR-1/global.json` to obtain LLVM IR.
2. Use `clang` (LLVM 15+) to compile the emitted IR alongside
   `testcases/IR-1/builtin/builtin.c` for the RISC-V target.
3. Execute the resulting assembly with the `reimu` RISC-V emulator, feeding the
   testcase’s `.in` input.
4. Compare the produced output with the testcase’s `.out` (ignoring blank lines
   and trailing whitespace for parity with the reference harness).

### Requirements

- `build/rcompiler` (run `cmake --build build` or `make build` first)
- `clang` 15+ with RISC-V support (`clang-17`, `clang-18`, etc.)
- The `reimu` RISC-V simulator on `PATH`

### Usage

```bash
# Build everything first
cmake --build build

# Run the entire IR-1 suite
./scripts/run_ir_tests.py

# Or via the Makefile helper
make ir-test

# Run a specific testcase with verbose diagnostics and keep temp folders
./scripts/run_ir_tests.py --tests comprehensive12 --verbose --keep-temps

# Specify alternative tool locations
./scripts/run_ir_tests.py --compiler build/rcompiler \
    --clang clang-18 --reimu /opt/bin/reimu
```

Useful flags:

- `--tests <name ...>`: limit execution to selected testpoints.
- `--keep-temps`: preserve the per-test temporary directories.
- `--verbose`: echo compiler diagnostics and include diffs in failures.
- `--fail-fast`: stop after the first failure.

When a testcase fails, the script prints the directory containing all of the IR,
assembly, and emulator transcripts so they can be inspected quickly.
