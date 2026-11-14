# Atomic / Thread / Memory Extensions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `std.atomic` with 16-byte/128-bit and POD aggregate atomics plus `AtomicRef`/`AtomicFlag`; add `std.thread` mutex/condition-variable wrappers; add `std.mem` placement construction and aligned-malloc aliases.

**Architecture:** The compiler’s existing `__cdd_atomic_*` lowering already uses integer carriers. Extend the carrier to `i128` and to POD aggregates whose alloc size is 1/2/4/8/16 bytes. Keep the legacy `atomic T` qualifier unchanged. Implement the public API entirely in `libs/std/*.cdd`; pthread types are opaque fixed-size storage in `std.thread`.

**Tech Stack:** C++20 compiler, LLVM 21, CDD standard library, GoogleTest e2e tests, pthread, mimalloc.

**Spec:** `docs/spec/stdlib.md` §§11, 18; `TODO.md` STD-05/08/24/25, CON-02/03/10.

## Global Constraints

- No hidden allocations or GC.
- `atomic T` qualifier stays restricted to 1/2/4/8-byte integer scalars.
- Aggregate atomics: only POD-like aggregates of size 1/2/4/8/16 bytes with natural alignment equal to size for 16-byte objects; reject larger/under-aligned/owning (`string`) aggregates.
- Public API uses explicit `init`/`destroy`; no constructors/destructors.
- Linux x86-64 primary target; add `+cx16` when the host supports it and link `libatomic` as a fallback.
- Keep existing 1901-test baseline green.

## Review Focus

- 16-byte atomic load/store/CAS must stay atomic when JIT or native code executes; do not accidentally lower to a non-atomic copy.
- Aggregate atomic carrier must preserve padding and size; bitcast to `iN` must be legal and aligned.
- `AtomicRef<T>` must not own or copy the referenced object.
- `Mutex`/`ConditionVariable` storage must be large enough and aligned for pthread types on Linux x86-64.
- `placement_alloc` must not return a pointer outside the caller-provided buffer.

---

### Task 1: Compiler 16-byte and aggregate atomic carrier

**Files:**
- Modify: `src/sema/CompileTimeEvaluator.h/.cpp` (public layout helper)
- Modify: `src/sema/SemanticAnalyzer.cpp` (atomic builtin validation)
- Modify: `src/codegen/CodegenContext.cpp/.h` (carrier type and width)
- Modify: `src/driver/CompilerDriver.cpp` (host `+cx16`, `-latomic` link)
- Modify: `CMakeLists.txt` / `src/CMakeLists.txt` (libatomic)
- Test: `tests/e2e/test_atomic.cpp`, `tests/e2e/test_std_atomic.cpp`

**Interfaces:**
- Produces: `SemanticAnalyzer::atomicCarrierBits(Type*, unsigned&)`; `CompileTimeEvaluator::layoutSizeAlign(Type*)`; `CodegenContext::getAtomicCarrierType` supports `Int128/UInt128/Float128` and POD aggregates of 1/2/4/8/16 bytes.

- [ ] Add failing builtin tests for `int128`, `float128`, 8-byte aggregate, and 16-byte aligned aggregate; add rejection tests for 12-byte/32-byte/under-aligned aggregates.
- [ ] Implement layout helper and aggregate POD validation.
- [ ] Extend carrier and value conversion for `i128`/aggregates.
- [ ] Add host `+cx16` target feature and `-latomic` fallback.
- [ ] Run focused atomic tests, then full suite.

### Task 2: `std.atomic` public surface

**Files:**
- Modify: `libs/std/atomic.cdd`
- Test: `tests/e2e/test_std_atomic.cpp`

**Interfaces:**
- Produces: `Atomic<T>` with 128-bit/aggregate support; `AtomicRef<T>` with `init(T*)`, `load`, `store`, `exchange`, CAS, fetch ops, `is_lock_free`, `is_always_lock_free`; `AtomicFlag` with `init`, `test_and_set`, `clear`, `test`.

- [ ] Add failing tests for `Atomic<int128>`, `Atomic<float128>`, `Atomic<Pair>` (8 bytes), `Atomic<AlignedQuad>` (16 bytes), `AtomicRef<int32>`, and `AtomicFlag`.
- [ ] Update `Atomic<T>::is_lock_free`/`is_always_lock_free` for 16-byte support.
- [ ] Add `AtomicRef<T>` delegating to existing free functions.
- [ ] Add `AtomicFlag`.
- [ ] Run focused tests.

### Task 3: `std.thread` mutex and condition variable

**Files:**
- Modify: `libs/std/thread.cdd`
- Test: `tests/e2e/test_std_thread.cpp`

**Interfaces:**
- Produces: `Mutex` (`init`, `destroy`, `lock`, `unlock`, `try_lock`, `error`), `ConditionVariable`/`Condvar` (`init`, `destroy`, `wait(Mutex*)`, `signal`, `broadcast`, `error`).

- [ ] Add failing tests for mutex-protected counter, try_lock/destroy, and condvar signal/broadcast.
- [ ] Implement opaque pthread storage wrappers.
- [ ] Run focused tests.

### Task 4: `std.mem` placement and aligned malloc

**Files:**
- Modify: `libs/std/mem.cdd`
- Test: `tests/e2e/test_std_mem.cpp`

**Interfaces:**
- Produces: `aligned_malloc(size, align)` and `alignmalloc(size, align)` aliases; `placement_new<T>(void*, T)`; `placement_alloc(void* buffer, usize capacity, usize size, usize align)`.

- [ ] Add failing tests for aliases, placement scalar/aggregate, and bounds/alignment.
- [ ] Implement.
- [ ] Run focused tests.

### Task 5: Docs and progress

**Files:**
- Modify: `docs/spec/stdlib.md`, `TODO.md`, `Progress.md`
- Test: full `ctest --test-dir build -j8 --output-on-failure`; `build/bin/cdd examples/all_features.cdd -o /tmp/all_features`.

- [ ] Update specs for new APIs and limitations.
- [ ] Update TODO/Progress.
- [ ] Run full suite and feature tour.
