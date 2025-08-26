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
├── whitelist.md
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

- [ ] Semantic Check (WIP)

- [ ] IR Codegen