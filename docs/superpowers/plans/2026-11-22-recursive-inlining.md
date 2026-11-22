# Recursive Inlining Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bounded, semantics-preserving self-recursive inlining pass and enable it in CDD's `-O2` pipeline, closing the `fib`/`nqueens` gaps without changing the default PIE/ASLR policy.

**Architecture:** A small LLVM module utility finds direct self-calls and mechanically inlines one call site per iteration with per-phase depth and hard instruction caps. `CompilerDriver` runs a pre-O2 selection pass, the existing O2 pipeline, then a post-O2 pass; it reruns the O2 pipeline only when a function actually changed.

**Tech Stack:** C++20, LLVM 22, GoogleTest, CMake, existing `benchmarks/run.py` conventions.

**Spec:** `docs/superpowers/specs/2026-11-22-pgo-recursion-layout-design.md`

## Global Constraints

- Build and test with LLVM 22 (`opt-22`, `llc-22`, `clang++-22`); do not downgrade the toolchain.
- Benchmark with `-O2`, 5 warmup + 9 measured runs, pinned CPU, separate tiers; `alloc` is excluded from core geomean.
- All benchmark checksums must match exactly.
- Default remains PIE; `-no-pie` stays opt-in.
- Do not use a git worktree; work in the current repository.
- Recursive inlining default is `-O2` only; `-fno-recursive-inlining` disables it.
- Pre phase: exactly one direct self-call and at most 100 instructions; depth 4.
- Post phase: pre-modified functions plus exactly-one-self-call functions with at most 100 instructions; depth 4.
- Inline-time cap: 1000 instructions. Never exceed it by speculative inlining; the later O2 rerun may grow the function, so measure final emitted size separately.
- One direct self-call per iteration, first in IR order; re-collect after every inline.
- The conditional O2 rerun must not re-run the recursive pass.
- PGO is out of scope for this plan; do not add PGO flags here.

## Review Focus

- A recursive function with two calls must not be silently expanded in the pre phase; the selector is part of correctness, not just size tuning.
- A function that reaches the 1000-instruction cap must stop before the next inline, not after it grows past the cap.
- `fib` must be handled by the post phase only; its pre-O2 two-call shape must be skipped.
- The O2 rerun must include CDD's existing non-trivial loop-unswitch block, not just LLVM's default O2 pipeline.
- Unchanged modules must not pay for a second O2 pipeline and must not change their emitted IR/code.

---

### Task 1: Recursive inlining utility and unit tests

**Files:**
- Create: `src/driver/RecursiveInlining.h`
- Create: `src/driver/RecursiveInlining.cpp`
- Create: `tests/driver/test_recursive_inlining.cpp`
- Modify: `src/CMakeLists.txt` (add LLVM `asmparser` component for IR parsing in tests)

**Interfaces:**
- Produces:
  ```cpp
  struct RecursiveInliningOptions {
      unsigned maxDepth = 4;
      unsigned maxInitialInstructions = 100;
      unsigned maxFinalInstructions = 1000;
  };

  struct RecursiveInliningResult {
      bool changed = false;
      llvm::SmallPtrSet<llvm::Function *, 8> modifiedFunctions;
  };

  RecursiveInliningResult inlineRecursiveCalls(
      llvm::Module &module,
      const RecursiveInliningOptions &options,
      llvm::ArrayRef<llvm::Function *> alsoAllow = {});
  ```
- `alsoAllow` bypasses both the single-self-call and initial-size filters; it is how the post phase continues a pre-modified function.
- A function with no direct self-call is never modified.
- The utility does not run optimization passes after inlining.

- [ ] **Step 1: Write the failing unit tests**

Create `tests/driver/test_recursive_inlining.cpp` with helpers that parse an IR string, count direct self-calls, and count instructions. Add tests:

```cpp
TEST(RecursiveInliningTest, SingleSelfCallGrowsButStaysRecursive) {
    const char *ir = R"(
        define i32 @rec(i32 %n) {
        entry:
          %base = icmp eq i32 %n, 0
          br i1 %base, label %done, label %step
        step:
          %m = add i32 %n, -1
          %r = call i32 @rec(i32 %m)
          %v = add i32 %r, 1
          ret i32 %v
        done:
          ret i32 0
        }
    )";
    RecursiveInliningOptions options;
    auto result = inlineRecursiveCalls(module, options);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(countSelfCalls(function), 1u);
    EXPECT_GT(function.getInstructionCount(), initialInstructions);
}

TEST(RecursiveInliningTest, TwoSelfCallsAreSkippedUnlessAllowed) {
    // A fib-shaped function with two direct self-calls.
    auto result = inlineRecursiveCalls(module, options);
    EXPECT_FALSE(result.changed);

    llvm::Function *fib = module.getFunction("fib");
    auto forced = inlineRecursiveCalls(module, options, {fib});
    EXPECT_TRUE(forced.changed);
}

TEST(RecursiveInliningTest, MaxFinalInstructionsStopsBeforeOverflow) {
    RecursiveInliningOptions options;
    options.maxFinalInstructions = 20;
    auto result = inlineRecursiveCalls(module, options);
    EXPECT_LE(function.getInstructionCount(), options.maxFinalInstructions);
}

TEST(RecursiveInliningTest, NoSelfCallIsUnchanged) {
    // A leaf function must produce changed == false and an empty modified set.
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='RecursiveInliningTest.*'
```
Expected: build fails because `RecursiveInlining.h` and the utility do not exist.

- [ ] **Step 3: Implement the utility**

In `RecursiveInlining.h`, declare the options, result, and function exactly as above. In `RecursiveInlining.cpp`:

- iterate only defined functions;
- build the target set from `alsoAllow`, otherwise require exactly one direct self-call and `getInstructionCount() <= maxInitialInstructions`;
- while depth `< maxDepth`, find the first direct self-call in IR order;
- stop if `F.getInstructionCount() * 2 > maxFinalInstructions` (self-inlining duplicates the body, so this is the conservative inline-time cap check);
- call `llvm::InlineFunction` with an `llvm::InlineFunctionInfo`, break on failure;
- insert `F` into `modifiedFunctions` and set `changed` on success;
- return the result.

Add `asmparser` to the existing `llvm_map_components_to_libnames` list in `src/CMakeLists.txt`.

- [ ] **Step 4: Run the focused tests to verify they pass**

Run:
```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='RecursiveInliningTest.*'
```
Expected: all four tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/driver/RecursiveInlining.h src/driver/RecursiveInlining.cpp \
        tests/driver/test_recursive_inlining.cpp src/CMakeLists.txt
git commit -m "feat: add bounded recursive inlining utility"
```

---

### Task 2: Driver flags and defaults

**Files:**
- Modify: `src/driver/CompilerDriver.h`
- Modify: `src/driver/CompilerDriver.cpp`
- Test: `tests/driver/test_compiler_driver.cpp`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces:
  ```cpp
  bool getRecursiveInlining() const;
  unsigned getRecursiveInliningDepth() const;
  ```
- Defaults: `recursiveInlining = true`, `recursiveInliningDepth = 4`.
- Flags: `-frecursive-inlining`, `-fno-recursive-inlining`, `-frecursive-inlining-depth=N`.
- The flags are parsed manually next to `-march`, because cxxopts treats single-dash long names as short-option clusters.

- [ ] **Step 1: Write failing parser tests**

Add to `tests/driver/test_compiler_driver.cpp`:

```cpp
TEST_F(CompilerDriverTest, RecursiveInliningIsOnByDefault) {
    const char* argv[] = {"cdd", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(2, argv));
    EXPECT_TRUE(driver.getRecursiveInlining());
    EXPECT_EQ(driver.getRecursiveInliningDepth(), 4u);
}

TEST_F(CompilerDriverTest, ParseNoRecursiveInlining) {
    const char* argv[] = {"cdd", "-fno-recursive-inlining", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_FALSE(driver.getRecursiveInlining());
}

TEST_F(CompilerDriverTest, ParseRecursiveInliningDepth) {
    const char* argv[] = {"cdd", "-frecursive-inlining-depth=2", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_EQ(driver.getRecursiveInliningDepth(), 2u);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='CompilerDriverTest.RecursiveInlining*:CompilerDriverTest.ParseNoRecursiveInlining'
```
Expected: compile failure because the getters/members do not exist.

- [ ] **Step 3: Add members, getters, parsing, and help text**

Add to `CompilerDriver.h`: `bool recursiveInlining = true; unsigned recursiveInliningDepth = 4;` and the two getters. Reset both at the top of `parseArguments`. Parse the three exact flag spellings; accept only decimal `unsigned` depth values and fail with `error: invalid -frecursive-inlining-depth` for malformed values. Add help lines:

```text
  -frecursive-inlining       Enable bounded self-recursive inlining at -O2 (default)
  -fno-recursive-inlining    Disable recursive inlining
  -frecursive-inlining-depth=N  Maximum self-inline depth (default: 4)
```

- [ ] **Step 4: Run the parser tests**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='CompilerDriverTest.*RecursiveInlining*'
```
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/driver/CompilerDriver.h src/driver/CompilerDriver.cpp \
        tests/driver/test_compiler_driver.cpp
git commit -m "feat: add recursive inlining driver flags"
```

---

### Task 3: Integrate pre/post recursion around the full O2 pipeline

**Files:**
- Modify: `src/driver/CompilerDriver.cpp`
- Test: `tests/driver/test_compiler_driver.cpp`

**Interfaces:**
- Consumes: `inlineRecursiveCalls`, `RecursiveInliningOptions`, `RecursiveInliningResult`.
- Produces:
  ```cpp
  static void runOptimizationPipeline(llvm::Module &module,
                                      llvm::TargetMachine &targetMachine,
                                      llvm::OptimizationLevel level,
                                      bool includeExtraUnswitch);
  ```
- `runOptimizationPipeline` contains both the default pipeline and the existing O2-only `LoopSimplify` + non-trivial `SimpleLoopUnswitch` + `InstCombine` + `SimplifyCFG` block. For recursion, call it with `O2` and `includeExtraUnswitch=true`; for the O2 rerun, use the same arguments.
- Pre-recursion and post-recursion use depth `recursiveInliningDepth`; pre uses no `alsoAllow`; post passes the pre-modified function list.
- Only `optLevel == 2` uses the pass. `-fno-recursive-inlining` emits exactly the old O2 path.

- [ ] **Step 1: Write the failing integration test**

Add to `tests/driver/test_compiler_driver.cpp`:

```cpp
TEST_F(CompilerDriverTest, RecursiveFibGrowsOnlyWhenEnabled) {
    const auto p = writeTempSource("cdd_rec_fib.cdd", R"(
int32 fib(int32 n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
int32 main() { println("{}", fib(10)); return 0; }
)");
    const std::string path = p.string();

    const char* on[] = {"cdd", "-S", "-O", "2", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(5, on));
    testing::internal::CaptureStdout();
    ASSERT_EQ(driver.run(), 0);
    const std::string withRec = testing::internal::GetCapturedStdout();

    const char* off[] = {"cdd", "-S", "-O", "2", "-fno-recursive-inlining",
                         path.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, off));
    testing::internal::CaptureStdout();
    ASSERT_EQ(driver.run(), 0);
    const std::string withoutRec = testing::internal::GetCapturedStdout();

    EXPECT_GT(withRec.size(), withoutRec.size());
    EXPECT_EQ(countOccurrences(withRec, "call i32 @fib"), 1u);
    std::filesystem::remove(p);
}
```

Also add a negative test that `-O1` with the flag does not change the emitted IR relative to plain `-O1` (the feature is O2-only). Add a leaf-function test that compares default `-O2` IR with `-O2 -fno-recursive-inlining` and expects byte-identical text, proving unchanged modules do not take a second optimization pipeline.

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='CompilerDriverTest.RecursiveFibGrowsOnlyWhenEnabled'
```
Expected: FAIL because the driver still uses the old inline O2 block.

- [ ] **Step 3: Refactor and integrate**

In `CompilerDriver.cpp`:

1. Extract the current `optLevel > 0` body into `runOptimizationPipeline`; keep the exact pass order and the `optLevel == 2` extra unswitch block. The helper creates its own analysis managers and `PassBuilder`, as the current code does.
2. Replace the inline body with `runOptimizationPipeline(...)`.
3. Before the helper, when `optLevel == 2 && recursiveInlining`:
   ```cpp
   RecursiveInliningOptions opts;
   opts.maxDepth = recursiveInliningDepth;
   auto pre = inlineRecursiveCalls(module, opts);
   ```
   Collect `pre.modifiedFunctions` into a `std::vector<llvm::Function*>`.
4. After the helper, when `optLevel == 2 && recursiveInlining`:
   ```cpp
   auto post = inlineRecursiveCalls(module, opts, preFunctions);
   ```
5. If `post.changed`, call `runOptimizationPipeline(..., O2, true)` exactly once more. Do not call `inlineRecursiveCalls` inside that rerun.
6. Keep `emitIR`, `compileOnly`, object emission, and linking order unchanged.

- [ ] **Step 4: Run integration and regression tests**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='CompilerDriverTest.RecursiveFibGrowsOnlyWhenEnabled:CompilerDriverTest.O2UnswitchesLoopInvariantBranches:CompilerDriverTest.StdlibPowerOfTwoIndexingAvoidsDivision'
ctest --test-dir build -j8 --output-on-failure
```
Expected: focused tests PASS; full suite remains green (expected count is the pre-change count plus the new tests).

- [ ] **Step 5: Commit**

```bash
git add src/driver/CompilerDriver.cpp tests/driver/test_compiler_driver.cpp
git commit -m "feat: enable bounded recursive inlining at O2"
```

---

### Task 4: Direct A/B gate, benchmarks, and documentation

**Files:**
- Create: `benchmarks/ab.py`
- Modify: `benchmarks/run.py` only if the existing harness needs a documented hook (prefer no change)
- Create: `benchmarks/results/2026-11-22-recursive-inlining-ab.md`
- Modify: `TODO.md`
- Modify: `Progress.md`

**Interfaces:**
- `benchmarks/ab.py` CLI:
  ```text
  python3 benchmarks/ab.py --old-cdd PATH --new-cdd PATH
      [--old-args "ARGS"] [--new-args "ARGS"]
      [--reps 9] [--warmup 5] [--cpu 10] [--only NAME]
      [--out PATH]
  ```
- It compiles every selected `benchmarks/cdd/bench_*.cdd`, runs the two binaries alternating, verifies identical stdout, and reports per-workload medians plus core/all geomeans.

- [ ] **Step 1: Add the A/B harness with a smoke check**

Implement `benchmarks/ab.py` using `subprocess` and `statistics`; copy the tier names from `benchmarks/run.py` (`core` excludes `alloc`). Add `--help`; smoke test:

```bash
python3 benchmarks/ab.py --help
python3 benchmarks/ab.py --old-cdd build/bin/cdd --new-cdd build/bin/cdd \
  --only bench_fib --reps 1 --warmup 1 --cpu 10 --out /tmp/ab-smoke.json
```
Expected: help works; smoke ratio is approximately 1.0.

- [ ] **Step 2: Build the old/new compilers for the gate**

Keep a pre-change compiler copy in `/tmp` before switching branches, or use `git worktree`? Do not use a worktree. Build the old compiler from `HEAD` (the commit before Task 1) in a temporary copy, and the new compiler from the working tree. Record the exact paths.

- [ ] **Step 3: Run the full direct A/B**

```bash
python3 benchmarks/ab.py --old-cdd /tmp/cdd-old/bin/cdd \
  --new-cdd build/bin/cdd --reps 9 --warmup 5 --cpu 10 \
  --out benchmarks/results/2026-11-22-recursive-inlining-ab.json
```
Expected: all 16 checksums match; core and all geomeans improve. If any workload regresses beyond noise, narrow the selector (size/depth) or make the feature opt-in and re-run.

- [ ] **Step 4: Run the full verification**

```bash
ctest --test-dir build -j8 --output-on-failure
./build/bin/cdd examples/all_features.cdd -o /tmp/all_features_rec
/tmp/all_features_rec | tail -1
```
Expected: full suite green; `ALL FEATURES OK`.

- [ ] **Step 5: Record results and update the progress log**

Write `benchmarks/results/2026-11-22-recursive-inlining-ab.md` with the per-workload table, core/all geomeans, checksum statement, exact filter/depth values, and the rejection reason for any workload that regressed. Add a progress snapshot to `TODO.md` and a dated section to `Progress.md`.

- [ ] **Step 6: Commit**

```bash
git add benchmarks/ab.py benchmarks/results/2026-11-22-recursive-inlining-ab.md \
        TODO.md Progress.md
git commit -m "docs: record recursive inlining A/B and default"
```
