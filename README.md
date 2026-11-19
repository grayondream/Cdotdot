# CDD

**English** | [简体中文](README_zh.md)

**CDD** (pronounced "C dot dot") is a statically typed, C-style systems
programming language compiled with LLVM. It keeps a C-style explicit memory
model and C-compatible FFI, removes the preprocessor and legacy numeric types,
and adds modules, namespaces, classes, single inheritance, templates, lambdas,
compile-time evaluation, atomics, threads, and a standard library written in
CDD itself.

[![Language: CDD](https://img.shields.io/badge/language-CDD-1f6feb)](https://github.com/grayondream/Cdotdot)
[![LLVM 18+](https://img.shields.io/badge/LLVM-18%2B-6f42c1)](https://llvm.org/)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)](https://isocpp.org/)
[![Platform: Linux x86-64](https://img.shields.io/badge/platform-Linux%20x86--64-lightgrey)](#platform-support)
[![Status: pre-1.0](https://img.shields.io/badge/status-pre--1.0-orange)](#status-and-limitations)
[![Tests: 1900+](https://img.shields.io/badge/tests-1900%2B-brightgreen)](#testing)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> **Project status: pre-1.0 / alpha.** Linux x86-64 is the only supported
> target. The language specification, ABI, and standard-library APIs are still
> being stabilized. Pin a commit if you need reproducible builds. See
> [Status and limitations](#status-and-limitations) and [TODO.md](TODO.md).

## Table of Contents

- [Overview](#overview)
- [Quick start](#quick-start)
- [Usage](#usage)
- [Language tour](#language-tour)
- [Standard library](#standard-library)
- [Repository layout](#repository-layout)
- [Architecture](#architecture)
- [Testing](#testing)
- [Documentation](#documentation)
- [Status and limitations](#status-and-limitations)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgements](#acknowledgements)

## Overview

CDD is designed for systems code that benefits from C's predictable execution
model but wants better structure than textual includes and macros. Source files
use the `.cdd` suffix. The public standard-library namespace is `std`; the
compiler's internal C++ namespace is `cdd`.

### At a glance

| Property | Value |
|---|---|
| Paradigm | Imperative / systems; procedural, object-oriented, generic |
| Typing | Static, nominal, fixed-width numeric types |
| Memory model | Manual, C-style; no garbage collector |
| Backend | LLVM 18+ |
| Primary target | Linux x86-64 |
| Source extension | `.cdd` |
| Standard-library namespace | `std` |
| Compiler implementation | C++20 |
| Status | Pre-1.0 / alpha |
| License | MIT; see [License](#license) |

### Design principles

- **Predictable execution** — fixed-width types (`int8` through `int128`,
  `float16` through `float128`), explicit allocation, no garbage collector, and
  no hidden control flow.
- **Explicit dependencies** — modules and imports instead of textual includes;
  `compile_time` and `compile_only` instead of macros.
- **Systems focus** — pointers, slices, inline assembly, atomics, threads, and
  C-compatible FFI.
- **Compile-time metaprogramming** — `compile_time` / `compile_only`,
  reflection, layout queries, and compile-time container evaluation.
- **CDD-native standard library** — the standard library is implemented in
  `libs/std/*.cdd`, not hidden inside the compiler.

### What CDD is not

- It is **not memory-safe** today: there is no borrow checker and no garbage
  collector. Manual memory management and undefined behavior are possible.
- It is **not a drop-in C++ replacement**: classes, inheritance, and templates
  are deliberate subsets; exceptions, RAII, virtual functions, and multiple
  inheritance are not part of the language.
- It is **not ABI-stable or cross-platform yet**. Linux x86-64 is the supported
  target.
- It is **not a 1.0 release**. The specification and implementation are still
  moving.

## Quick start

### Requirements

| Requirement | Notes |
|---|---|
| Linux x86-64 | Primary and currently only supported target |
| CMake 3.20+ | Build system |
| C++20 compiler | GCC or Clang |
| LLVM 18+ | Tested with LLVM 22.1.8 |
| `vcpkg` (optional) | Provides LLVM, `spdlog`, and `cxxopts` when configured |
| GoogleTest | Fetched automatically if not installed |
| `pthread` | Linked by the driver |
| mimalloc | Vendored in `thirdparty/mimalloc` |

### Build

```bash
git clone https://github.com/grayondream/Cdotdot.git
cd Cdotdot
./scripts/build.sh
```

For a release build:

```bash
BUILD_TYPE=Release ./scripts/build.sh
```

Or configure manually:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build build -j"$(nproc)"
```

If `llvm-config` is not on `PATH`, set `LLVM_DIR` to the directory containing
`LLVMConfig.cmake`. If you use `vcpkg`, set `VCPKG_ROOT` before running
`./scripts/build.sh`.

The compiler is written to `build/bin/cdd`.

### Install (optional)

```bash
cmake --install build --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
```

The compiler is installed together with the standard library under
`share/cdd/libs`. At runtime, the standard library is resolved relative to the
compiler executable; `CDD_STD_DIR` can override the search path.

### Hello, CDD

Create `hello.cdd`:

```cdd
module hello;

int32 main() {
    println("Hello, CDD!");
    return 0;
}
```

Compile and run:

```bash
build/bin/cdd hello.cdd -o hello
./hello
```

Expected output:

```text
Hello, CDD!
```

A source file's `module` declaration must match its file name: `module hello;`
lives in `hello.cdd`.

### Run the test suite

```bash
ctest --test-dir build -j"$(nproc)" --output-on-failure
```

Run the end-to-end feature tour:

```bash
build/bin/cdd examples/all_features.cdd -o /tmp/all_features
/tmp/all_features
```

The last line must be `ALL FEATURES OK`.

## Usage

```text
cdd [options] <input.cdd>
```

| Option | Description |
|---|---|
| `-c` | Compile to an object file; do not link |
| `-o <file>` | Set the output path |
| `-S` | Emit LLVM IR instead of an executable |
| `-M <path>` | Add a module import search path (repeatable) |
| `-O <0-3>` | Set the optimization level |
| `-g` | Emit debug information |
| `-v` | Enable verbose driver output |
| `--no-prelude` | Do not inject the automatic `std.c` libc binding layer |
| `-Wall` | Enable all warnings |
| `-Werror` | Treat warnings as errors |
| `-fsyntax-only` | Run lexer, parser, import, and semantic checks; skip codegen |
| `-l <lib>` | Link a library (repeatable) |
| `-L <path>` | Add a library search path (repeatable) |
| `-h`, `--help` | Show the command-line help |

Examples:

```bash
# Parse and import-check only
build/bin/cdd -fsyntax-only hello.cdd

# Emit LLVM IR
build/bin/cdd -S hello.cdd -o hello.ll

# Compile to an object file
build/bin/cdd -c hello.cdd -o hello.o

# Optimized debug build
build/bin/cdd hello.cdd -o hello -O 2 -g
```

Run `build/bin/cdd --help` for the full option list.

### Modules

CDD has no textual includes. A file declares exactly one module, and imports use
dotted module paths:

```cdd
module app;
import std.vec;
import std.io;
```

Imports are resolved from the importing file's directory, paths supplied with
`-M`, and the installed standard library. `CDD_STD_DIR` can override the
standard-library root. The `std.c` libc binding layer is injected automatically
unless `--no-prelude` is passed. Legacy `.smc` sources are not supported and are
rejected with a migration hint.

### Standard-library example

```cdd
module vec_demo;
import std.vec;

int32 main() {
    std::Vec<int32> v = {};
    v.init();
    v.push(42);
    println("{}", v.get(0));
    v.destroy();
    return 0;
}
```

Owning containers and synchronization primitives use explicit `init` /
`destroy` lifecycles. There are no destructors or hidden cleanup paths.

## Language tour

The following program is a complete, compilable CDD file:

```cdd
module tour;

class Counter {
public:
    int32 count;
    void inc() { this->count = this->count + 1; }
};

template<typename T> struct Box { T value; };

compile_time int32 twice(int32 x) { return x * 2; }

int32 main() {
    Counter c;
    c.count = 0;
    c.inc();

    Box<int32> b;
    b.value = twice(c.count);
    println("{}", b.value);
    return 0;
}
```

It prints `2`.

Other implemented language areas include:

- Fixed-width integer and floating-point types, including `int128`,
  `uint128`, `float16`, and `float128`.
- Modules, namespaces, `struct` / `class` / `union` / `enum`, access control,
  single inheritance, bit-fields, and anonymous types.
- Class, function, alias, and non-type templates, plus CRTP-style static
  dispatch.
- Lambdas and closures, function and operator overloading, default arguments.
- Arrays, slices (`T[]`), `optional<T>`, `result<T, E>`, `defer`, designated
  initializers, flexible array members, and C-style literals.
- Inline assembly, atomics, threads, mutexes, condition variables, and C FFI.
- `compile_time` / `compile_only`, reflection, layout queries, target/build
  information, and compile-time container evaluation.

See [`examples/all_features.cdd`](examples/all_features.cdd) for the end-to-end
feature tour.

## Standard library

The standard library lives in `libs/std/` and is imported by dotted module name.
Public names are exposed through the `std` namespace.

| Area | Modules |
|---|---|
| Core and runtime | `std.c`, `std.core`, `std.io`, `std.mem`, `std.bit`, `std.math` |
| Text | `std.string`, `std.ascii`, `std.parse`, `std.path` |
| Containers and algorithms | `std.vec`, `std.deque`, `std.hashmap`, `std.hashset`, `std.hash`, `std.sort` |
| System and concurrency | `std.fs`, `std.os`, `std.time`, `std.thread`, `std.atomic` |
| Testing | `std.testing` |

The library is written in CDD. Container and threading APIs use explicit
resource lifecycles; errors are reported through status values or panics rather
than exceptions.

## Repository layout

| Path | Contents |
|---|---|
| `src/frontend/` | Lexer, token stream, parser, operator precedence |
| `src/ast/` | AST nodes, types, layout, mangling, compile-time values |
| `src/sema/` | Name/type resolution, templates, compile-time evaluation, reflection, diagnostics |
| `src/codegen/` | LLVM IR generation |
| `src/driver/` | CLI, module loading, standard-library prelude, linking |
| `libs/std/` | Standard library written in CDD |
| `examples/` | `all_features.cdd`, the end-to-end feature tour |
| `tests/` | Unit, integration, e2e, driver, and specification-conformance tests |
| `benchmarks/` | Performance baseline (CDD vs clang++/g++, runtime, compile time, code size) |
| `docs/spec/` | Normative language specification |
| `docs/superpowers/` | Design notes and historical implementation plans |
| `scripts/` | Build scripts |
| `thirdparty/mimalloc/` | Vendored allocator |
| `resources/` | Test resources and historical C comparison samples |

## Architecture

```text
.cdd source
    |
    v
Lexer -> Parser -> AST -> Semantic analysis -> LLVM IR -> object -> link
                        |
                        +-- compile_time evaluator / reflection
                        +-- layout and name mangling
```

- `CompilerDriver` orchestrates compilation, module preloading, standard-library
  injection, optimization, and linking.
- `ModuleLoader` resolves `.cdd` imports; `StdPrelude` injects the `std.c` libc
  binding layer.
- `SemanticAnalyzer` resolves names, types, overloads, and templates;
  `CompileTimeEvaluator` evaluates `compile_time` code.
- `CodegenContext` lowers the checked AST to LLVM IR. `LayoutBuilder` computes
  type layout and `Mangle` produces symbol names.
- `Linker` invokes the system linker through `cc` and links mimalloc, `pthread`,
  and any user-requested libraries.
- The test suite uses LLVM's in-process LLJIT for execution tests.

The compiler is implemented in C++20 and uses `cdd` as its internal C++
namespace.

## Testing

The repository contains unit, integration, driver, codegen, and end-to-end
tests, including a specification-conformance suite. The current tree has more
than 1,900 passing tests.

```bash
# Full suite
ctest --test-dir build -j"$(nproc)" --output-on-failure

# End-to-end feature tour
build/bin/cdd examples/all_features.cdd -o /tmp/all_features
/tmp/all_features
```

The feature tour must exit with status 0 and print `ALL FEATURES OK` as its last
line.

The performance baseline lives in [`benchmarks/`](benchmarks/README.md). A quick
smoke run is:

```bash
python3 benchmarks/run.py --quick
```

The full baseline and its methodology are documented in
[`benchmarks/BASELINE.md`](benchmarks/BASELINE.md).

Automated CI is not configured in this repository yet; verification is
currently local. See [TODO.md](TODO.md) for the test/CI backlog.

## Documentation

| Document | Contents |
|---|---|
| [`README_zh.md`](README_zh.md) | Simplified Chinese README |
| [`docs/spec/README.md`](docs/spec/README.md) | Specification index and status markers |
| [`docs/spec/grammar.ebnf`](docs/spec/grammar.ebnf) | Grammar and operator precedence |
| [`docs/spec/keywords.md`](docs/spec/keywords.md) | Keywords, reserved words, operators |
| [`docs/spec/modules.md`](docs/spec/modules.md) | Modules, namespaces, import visibility |
| [`docs/spec/semantics.md`](docs/spec/semantics.md) | Evaluation, overloading, static checks |
| [`docs/spec/conversions.md`](docs/spec/conversions.md) | Conversions, promotions, overload ranks |
| [`docs/spec/abi.md`](docs/spec/abi.md) | Type layout, calling convention, name mangling |
| [`docs/spec/compile_time.md`](docs/spec/compile_time.md) | `compile_time` syntax, evaluator, reflection |
| [`docs/spec/stdlib.md`](docs/spec/stdlib.md) | Builtins and standard-library reference |
| [`docs/spec/fixed-width-types.md`](docs/spec/fixed-width-types.md) | Fixed-width numeric type model |
| [`TODO.md`](TODO.md) | Engineering backlog and roadmap |
| [`Progress.md`](Progress.md) | Chronological implementation record |

## Status and limitations

### Implemented

The current tree includes fixed-width numeric types, modules and namespaces,
classes with single inheritance, structs/unions/enums and bit-fields, templates
(class/function/alias/non-type), lambdas and closures, operator overloading,
slices, `optional` / `result`, designated initializers, flexible array members,
inline assembly, C FFI, `compile_time` / `compile_only`, reflection,
atomics/threads/mutexes/condition variables, and 22 standard-library modules.

### Known limitations

- **Platform:** Linux x86-64 only. Other Linux architectures are untested;
  macOS and Windows are not supported yet.
- **ABI:** the ABI is documented but not frozen. Aggregate by-value passing and
  returning, name mangling, and C FFI edge cases are still being stabilized.
- **Memory safety:** manual memory management, no RAII, no destructors, no
  exceptions, and no garbage collector. Bounds and overflow behavior are not yet
  fully specified.
- **Language surface:** some C++-style syntax extensions are still being
  reconciled with the grammar; templates are a practical subset, not full C++
  template metaprogramming.
- **Tooling:** the compiler is a minimal CLI. Package management, LSP, formatter,
  and documentation generation are explicit non-goals for now.
- **Verification:** no CI workflow is configured; the suite is currently run
  locally.

### Non-goals

CDD deliberately excludes:

- the C preprocessor, textual includes, and macros;
- `goto` and labels;
- variable-length arrays;
- multiple and virtual inheritance, virtual functions, vtables, and RTTI;
- exceptions and stack unwinding;
- garbage collection and ownership/borrow checking;
- template specialization, SFINAE, variadic templates, template-template
  parameters, concepts, and default template arguments;
- package management, LSP, formatter, and documentation generators.

`constexpr` has been removed from the language. Use `compile_time` for code that
can run at compile time and at runtime, and `compile_only` for compile-time-only
functions.

### Platform support

| Platform | Status |
|---|---|
| Linux x86-64 | Supported (primary target) |
| Other Linux architectures | Untested |
| macOS | Not supported yet |
| Windows | Not supported yet |

### Versioning

CDD is pre-1.0. A stable language-version flag is planned but not yet available;
the specification and ABI may change between commits. For reproducible builds,
pin a commit or tag.

## Contributing

Contributions are welcome, especially bug reports, test cases, documentation
fixes, and focused implementation work.

1. Read [`TODO.md`](TODO.md) and [`Progress.md`](Progress.md) before starting
   substantial work.
2. Open an issue for large changes so the design and scope can be discussed.
3. Add tests for new behavior. The project follows a test-first workflow where
   practical.
4. Keep `docs/spec/` in sync with parser, semantic, or ABI changes.
5. Run the full test suite and the feature tour before submitting a pull
   request.
6. Use focused commit messages such as `feat(AREA): ...`, `fix(AREA): ...`, or
   `docs: ...`.

Bug reports should include the platform, LLVM version, `cdd -v` output, and a
minimal `.cdd` reproducer.

## License

CDD is released under the MIT License. See [`LICENSE`](LICENSE) for the full
license text.

## Acknowledgements

CDD builds on LLVM and uses mimalloc as its allocator backend. The test suite
uses GoogleTest; the command-line driver uses cxxopts and spdlog. vcpkg is
supported for dependency management. See [`thirdparty/`](thirdparty/) and
[`vcpkg.json`](vcpkg.json) for details.
