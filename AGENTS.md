# RCompiler Technical Overview

## Project Overview

RCompiler is a C++ implementation of a compiler for a subset of the Rust programming language.

**Supported Language Subset**: Key restrictions include
- Integer types only: `i32`, `u32`, `isize`, `usize` (no floating-point)
- No closures, async/await, or try operators
- No `for` loops (only `loop` and `while`)
- ASCII-only identifiers
- Pattern matching with strict irrefutability rules

**Current Status**: Lexer, Parser, and Semantic Analysis are complete. IR code generation is in progress.

---

## Architecture & Pipeline

The compiler follows a traditional multi-pass pipeline:

```
Source Code → Preprocessor → Lexer → Parser → AST → Semantic Analyzer → IR → (Future: Codegen)
```

### 1. Preprocessor (`include/preprocessor/`)

**Purpose**: Sanitize source input before lexical analysis.

**Operations**:
- **Comment removal**: Strip line (`//`) and block (`/* */`) comments while preserving string/char literal integrity
- **Whitespace normalization**: Collapse consecutive spaces, handle line continuations in strings
- **ASCII validation**: Enforce strict ASCII-only constraint, rejecting any byte > 0x7F

**Design Concerns**:
- State machine tracking for nested block comments and escape sequences
- Proper handling of edge cases (comments in strings, escaped quotes)
- Line/column tracking for error reporting

**Usage**: Instantiate with filename (or empty string for stdin), call `preprocess()` for cleaned output.

---

### 2. Lexer (`include/lexer/`)

**Purpose**: Tokenize preprocessed source into a token stream.

**Token Generation**:
- **Keywords**: Defined via X-macro pattern in `token_defs.def` for maintainability
- **Operators**: Multi-character aware (e.g., `>>=` parsed before `>>`)
- **Literals**: Raw strings (with arbitrary `#` delimiters), byte strings, C strings, integers (with base/suffix detection)
- **Identifiers**: ASCII alphanumeric + underscore, cannot start with underscore

**Design Concerns**:
- **Raw string complexity**: Handling `r#"..."#` with matching delimiter counts
- **Greedy operator matching**: Longest-match disambiguation (e.g., `<<=` vs `<` + `<=`)
- **Integer validation**: Regex-based post-pass to verify literal format correctness

**Implementation Pattern**: Single-pass scanner with lookahead for multi-char tokens. Uses regex for keyword classification after initial tokenization.

---

### 3. Parser (`include/ast/parser.hpp`)

**Architecture**: Hybrid combinator + Pratt parser.

**Parser Combinators** (`include/ast/utils/parsec.hpp`):
- Monadic parser composition with `map`, `combine`, `thenL`, `thenR`
- Backtracking via saved position snapshots
- Type-safe with templates and `std::optional` for failure representation

**Pratt Parser** (`include/ast/utils/pratt.hpp`):
- Handles expression precedence and associativity
- Prefix/infix/postfix operator registration with binding power
- Delegated to parsec combinators for complex expressions (blocks, if, loop)

**Pattern Parser** (`include/ast/parsers/patternParser.hpp`):
- Separate sub-parser for pattern matching constructs
- Handles identifier, tuple, struct, and enum patterns
- Enforces irrefutability constraints at parse time where possible

**AST Nodes** (`include/ast/nodes/`):
- Visitor pattern for traversal (`BaseVisitor` with virtual dispatch)
- Four categories: Expressions (`expr.hpp`), Statements (`stmt.hpp`), Items (`topLevel.hpp`), Patterns (`pattern.hpp`)
- Base class hierarchy rooted in `BaseNode` with `accept()` method

**Design Concerns**:
- **Pratt integration**: Careful delegation to avoid infinite recursion between Pratt and parsec
- **Ambiguity resolution**: Struct expressions vs. path expressions require lookahead (resolved via try-parse pattern)
- **Error recovery**: Currently minimal—parser fails on first error without recovery

**Key Patterns**:
- Item parsers composed with `|` alternation
- Statement parsing tries `let` → `empty` → `expr` with backtracking
- Expression parsing delegates to Pratt table, which calls back to parsec for non-operator constructs

---

### 4. Semantic Analysis (`include/semantic/`)

**Multi-Pass Architecture**: Semantic analysis occurs in 6 distinct passes, each building on previous results.

#### **Pass 1: Scope Construction** (`analyzer/firstPass.hpp`)

**Purpose**: Build hierarchical scope tree and collect item names.

**Operations**:
- Create `ScopeNode` tree mirroring AST structure
- Register all items (functions, structs, enums, constants) in namespaces
- Separate value namespace (functions, constants) from type namespace (structs, enums, traits)
- Populate with builtin functions (`analyzer/builtin.hpp`)

**Scope Management** (`scope.hpp`):
- `ScopeNode`: Tree structure with parent links
- `CollectedItem`: Item metadata (name, kind, AST reference)
- Dual namespace design (value/type) to handle Rust's namespace rules

**Output**: `root_scope` with complete symbol table.

#### **Pass 2: Type Resolution** (`analyzer/secondPass.hpp`)

**Purpose**: Resolve all type annotations and evaluate constant expressions.

**Operations**:
- Convert `LiteralType` (parser-level types) to `SemType` (semantic types)
- Process constant items first to enable their use in type expressions
- Resolve struct/enum definitions and populate metadata
- Handle array size expressions via constant evaluator

**Constant Evaluation** (`analyzer/constEvaluator.hpp`):
- Compile-time expression evaluation for array sizes, const initializers
- Supports integer arithmetic, casts, struct/array/tuple construction
- Enforces const-correctness rules (no mutable references, no side effects)
- Returns `ConstValue` with storage and type information

**Type System** (`types.hpp`):
- `SemType`: Discriminated union for primitives, tuples, arrays, slices, references, named items
- `SemPrimitiveKind`: Enumeration of base types plus `ANY_INT` for unsuffixed literals
- Reference semantics: Tracks mutability (`&T` vs `&mut T`)

**Design Concerns**:
- **Recursive types**: Not yet supported (no forward declaration mechanism)
- **Type inference**: Limited to integer literals; explicit annotations required elsewhere
- **Const evaluator complexity**: Must mirror runtime semantics while enforcing compile-time restrictions

#### **Pass 3: Impl Promotion** (`analyzer/thirdPass.hpp`)

**Purpose**: Promote `impl` block methods to struct-level metadata.

**Operations**:
- Find inherent `impl` blocks (`impl StructName { ... }`)
- Extract methods and associated constants
- Attach to corresponding struct's `StructMetaData::methods`

**Design Concerns**:
- **Trait impl handling**: Currently minimal (trait impls recognized but not fully processed)
- **Name resolution**: Only supports simple paths (single identifier); qualified paths not yet implemented

#### **Pass 4: Type Checking** (`analyzer/fourthPass.hpp`)

**Purpose**: Full type checking, pattern binding, and mutability analysis.

**Operations**:
- **Expression type checking**: Bottom-up inference with caching
- **Pattern binding**: Destructuring with irrefutability verification
- **Place expression analysis**: Determine writability for assignments
- **Borrow checking**: Track mutability of references and validate usage
- **Control flow typing**: `break` expressions and `return` type consistency

**Binding Stack**:
- `BindingFrame`: Maps identifiers to `IdentifierMeta` (type, mutability, ref status)
- Pushed/popped on scope entry/exit
- Handles shadowing via stack lookup

**Place Analysis**:
- `PlaceInfo`: Tracks whether expression is a valid assignment target
- Validates writability: mutable binding, dereference of `&mut`, field of writable place

**Design Concerns**:
- **Borrow checker limitations**: Simplified compared to rustc; no lifetimes or region inference
- **Type inference**: Mostly explicit; `ANY_INT` resolved to `i32` or context-dependent type
- **Error messages**: Basic; could benefit from span information and suggestions

#### **Pass 5: Control Flow Analysis** (`analyzer/controlAnalyzer.hpp`)

**Purpose**: Validate control flow constructs and return path analysis.

**Operations**:
- Verify `break`/`continue` only appear inside loops
- Check function return paths (all code paths return or diverge)
- Validate `break` expression types match loop type context

**Design Concerns**:
- **Divergence tracking**: `!` (never) type propagation for type checking
- **Match exhaustiveness**: Not yet implemented

#### **Pass 6: Dirty Work** (`analyzer/dirtyWorkPass.hpp`)

**Purpose**: Handle spec-violating edge cases required for test suite compatibility.

**Operations**:
- Special handling for `exit(0)` in main function (treated as diverging)
- Workarounds for undefined behavior in test cases

---

### 5. IR Generation (`include/ir/`)

**Status**: Skeleton code exists; not yet implemented.

**Planned Architecture** (`ir/gen.hpp`, `ir/instructions/`):
- SSA-based intermediate representation
- Visitor pattern for IR traversal (`visit.hpp`)

---

## Key Design Patterns

### Visitor Pattern

Used throughout for AST/IR traversal. Base class `BaseVisitor` with virtual methods for each node type. Allows separation of concerns:
- Pretty-printing (`ast/visitors/pretty_print.hpp`)
- Semantic passes (each pass is a visitor)
- Future: IR generation, optimization passes

### Type System

Two-level type representation:
- `LiteralType` (parser): Syntax-level types with unresolved references
- `SemType` (semantic): Fully resolved types with metadata links

Conversion in Pass 2 ensures separation of parsing and semantic concerns.

---

## Testing Infrastructure

**Test Organization** (`tests/`):
- `ci_preprocessor_tests.cpp`: Preprocessor unit tests
- `ci_lexer_tests.cpp`: Lexer integration tests
- `ci_parser_tests.cpp`: Parser integration tests

**Test Data** (`ci/files/`, `ci/expected_output/`):
- Input Rust files with expected token streams or parse trees
- Negative tests for error cases

**Build System**: CMake with CTest integration. Run via `make test` or `ctest`.

---

## Build & Usage

**Dependencies**: C++20 compiler (GCC/Clang), CMake 3.16+

**Build**:
```bash
mkdir -p build && cd build
cmake ..
make
```

**Usage**:
```bash
./build/rcompiler <source.rs>
# Or stdin: ./build/rcompiler < source.rs
```

**Logging**: Controlled via `LOGGING_LEVEL_*` macros in `utils/logger.hpp`. Set in `main.cpp` or compile flags.
