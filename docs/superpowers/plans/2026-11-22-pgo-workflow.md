# PGO Workflow and Function Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an opt-in, reproducible IR-PGO training/use workflow and a measured profile-guided function-ordering experiment, without changing any default compilation behavior.

**Architecture:** The driver instruments optimized-input IR with LLVM's `PGOInstrumentationGen`, lowers the profile runtime after optimization, and links `libclang_rt.profile`. `-fprofile-use` runs `PGOInstrumentationUse` before optimization; block layout follows LLVM's profile-aware passes. A separate stable sort reorders IR functions by entry count only when `-fprofile-layout` is explicit.

**Tech Stack:** C++20, LLVM 22, Clang compiler-rt profile runtime, GoogleTest, CMake, Python 3 training helper.

**Spec:** `docs/superpowers/specs/2026-11-22-pgo-recursion-layout-design.md`

**Prerequisite:** Execute `docs/superpowers/plans/2026-11-22-recursive-inlining.md` first. This plan uses its `runOptimizationPipeline` helper and pre/post recursive hook.

## Global Constraints

- PGO is opt-in. No default profile generation, profile use, or function reordering.
- Default remains PIE; no change to `-no-pie` or ASLR policy.
- Profile generation must run before optimization; profile use must run before optimization; instrumentation lowering must run after optimization and before object emission.
- Profile output is absolute: `DIR/default_%p.profraw`; create `DIR` and never rely on the program working directory.
- A missing compiler-rt profile archive is a hard error for `-fprofile-generate`; tests skip only when the test runtime is unavailable.
- No LLD dependency in this milestone. LLD order files are a fallback only if IR sorting fails its A/B gate.
- PGO A/B uses the same 5 warmup + 9 measured runs, pinned CPU, checksum equality, and tier rules as the recursive plan.
- Do not run a second optimization pipeline merely because profile mode is enabled.
- `-fprofile-layout` must be off until its A/B gate passes.

## Review Focus

- Instrumentation counters must remain correct when optimization and recursive inlining duplicate or delete instrumented blocks.
- A profile created from one process must not be overwritten by another process; `%p` and the absolute output directory are correctness requirements.
- A profile-use build must fail clearly on a missing/unreadable profile, not silently fall back to no profile.
- Function sorting must be stable and must not move declarations, aliases, or `main` in a way that changes the entry symbol.
- Unchanged PGO-off and profile-layout-off builds must produce the same flags and code as before.

---

### Task 1: Runtime discovery, linker arguments, and flags

**Files:**
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `src/driver/CompilerDriver.h`
- Modify: `src/driver/CompilerDriver.cpp`
- Modify: `src/driver/Linker.h`
- Modify: `src/driver/Linker.cpp`
- Test: `tests/driver/test_pgo.cpp`

**Interfaces:**
- Produces driver getters:
  ```cpp
  bool getProfileGenerate() const;
  const std::string& getProfileGenerateDir() const;
  const std::string& getProfileUseFile() const;
  bool getProfileLayout() const;
  ```
- Produces linker argument:
  ```cpp
  std::string constructLinkCommand(const std::vector<std::string>& objects,
                                   const std::string& output,
                                   const std::vector<std::string>& libs = {},
                                   const std::vector<std::string>& libPaths = {},
                                   bool pie = true,
                                   const std::vector<std::string>& extraArgs = {});
  int link(..., bool pie = true,
           const std::vector<std::string>& extraArgs = {});
  ```
- `extraArgs` are emitted after objects and before `-L`/`-l`; use them for
  `-u__llvm_profile_runtime` and the profile archive.
- CMake detects `libclang_rt.profile-x86_64.a` via `clang-22`/`clang -print-resource-dir`, stores it as cache variable `CDD_PROFILE_RUNTIME`, and defines it on `cdd_lib`. Tests receive `CDD_TEST_PROFILE_RUNTIME` only when found.

- [ ] **Step 1: Write failing parser and linker tests**

Create `tests/driver/test_pgo.cpp` with parser tests:

```cpp
TEST_F(PgoDriverTest, PgoIsOffByDefault) {
    const char* argv[] = {"cdd", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(2, argv));
    EXPECT_FALSE(driver.getProfileGenerate());
    EXPECT_TRUE(driver.getProfileGenerateDir().empty());
    EXPECT_TRUE(driver.getProfileUseFile().empty());
    EXPECT_FALSE(driver.getProfileLayout());
}

TEST_F(PgoDriverTest, ParseProfileGenerateDirectory) {
    const char* argv[] = {"cdd", "-fprofile-generate=/tmp/pgo", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getProfileGenerate());
    EXPECT_EQ(driver.getProfileGenerateDir(), "/tmp/pgo");
}

TEST_F(PgoDriverTest, ParseBareProfileGenerateUsesPgoDirectory) {
    const char* argv[] = {"cdd", "-fprofile-generate", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_EQ(driver.getProfileGenerateDir(), "pgo");
}

TEST_F(PgoDriverTest, ParseProfileUseAndLayout) {
    const char* argv[] = {"cdd", "-fprofile-use=/tmp/x.profdata",
                          "-fprofile-layout", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    EXPECT_EQ(driver.getProfileUseFile(), "/tmp/x.profdata");
    EXPECT_TRUE(driver.getProfileLayout());
}
```

Add to `tests/driver/test_linker.cpp`:

```cpp
TEST(LinkerTest, ConstructLinkCommandAppendsExtraArgsBeforeLibraries) {
    Linker linker;
    std::string cmd = linker.constructLinkCommand(
        {"main.o"}, "app", {"m"}, {}, true,
        {"-u__llvm_profile_runtime", "/opt/libclang_rt.profile-x86_64.a"});
    EXPECT_LT(cmd.find("main.o"), cmd.find("-u__llvm_profile_runtime"));
    EXPECT_LT(cmd.find("-u__llvm_profile_runtime"), cmd.find("-lm"));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='PgoDriverTest.*:LinkerTest.ConstructLinkCommandAppendsExtraArgsBeforeLibraries'
```
Expected: compile failure because the flags/getters/extra argument do not exist.

- [ ] **Step 3: Implement detection, flags, and linker arguments**

In `src/CMakeLists.txt`, detect the archive with `find_program(CDD_CLANG_EXECUTABLE NAMES clang-22 clang)`, `execute_process(COMMAND ... -print-resource-dir)`, and `file(GLOB ...)`. Use a cache variable so `tests/CMakeLists.txt` can see it:
```cmake
set(CDD_PROFILE_RUNTIME "${CDD_PROFILE_RUNTIME}" CACHE INTERNAL
    "compiler-rt profile runtime archive")
```
Define `CDD_PROFILE_RUNTIME` on `cdd_lib` when non-empty; define `CDD_TEST_PROFILE_RUNTIME` on `compiler_tests` when non-empty.

In `CompilerDriver.h`, add the four getters and members:
```cpp
bool profileGenerate = false;
std::string profileGenerateDir;
std::string profileUseFile;
bool profileLayout = false;
```
Reset all four at the start of `parseArguments`. Parse manually:
- `-fprofile-generate` => generate=true, dir=`pgo`;
- `-fprofile-generate=DIR` => generate=true, dir=DIR (empty DIR is an error);
- `-fprofile-use=FILE` => useFile=FILE (empty FILE is an error);
- `-fprofile-layout` => layout=true.

Add help lines for all four. `profileGenerate` is rejected in `run()` with a clear error if `CDD_PROFILE_RUNTIME` is not defined.

Extend `Linker` exactly as the interface above; insert `extraArgs` in the command after objects and before `-L`.

- [ ] **Step 4: Run the focused tests**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='PgoDriverTest.*:LinkerTest.ConstructLinkCommandAppendsExtraArgsBeforeLibraries'
```
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/CMakeLists.txt tests/CMakeLists.txt \
        src/driver/CompilerDriver.h src/driver/CompilerDriver.cpp \
        src/driver/Linker.h src/driver/Linker.cpp \
        tests/driver/test_pgo.cpp tests/driver/test_linker.cpp
git commit -m "feat: add PGO flags and profile runtime discovery"
```

---

### Task 2: Instrumentation generation, profile use, and linking

**Files:**
- Modify: `src/driver/CompilerDriver.cpp`
- Test: `tests/driver/test_pgo.cpp`

**Interfaces:**
- Consumes: `getProfileGenerate()`, `getProfileGenerateDir()`, `getProfileUseFile()`, `Linker::link(..., extraArgs)`.
- Produces a driver pipeline with these exact passes:
  - generate mode: `PGOInstrumentationGen` before `runOptimizationPipeline`, `InstrProfilingLoweringPass` after it;
  - use mode: `PGOInstrumentationUse(profileUseFile)` before `runOptimizationPipeline`;
  - generate mode link: `extraArgs = {"-u__llvm_profile_runtime", CDD_PROFILE_RUNTIME}`.
- `-fprofile-generate` is accepted at `-O0` through `-O3`; profile use is accepted at the same levels, but layout passes only affect optimized code.
- PGO generation/use runs before the recursive pre phase. The recursion pass therefore cannot invalidate profile lookup, and the profile-selected code is already present when recursive inlining runs.
- `-S` with generate prints instrumented IR and does not link.

- [ ] **Step 1: Write failing integration tests**

Add to `tests/driver/test_pgo.cpp`:

```cpp
TEST_F(PgoDriverTest, GenerateEmitsProfileCountersInIR) {
    const auto p = writeTempSource("cdd_pgo_ir.cdd", R"(
int32 main() { int32 x = 1; if (x) { x = 2; } println("{}", x); return 0; }
)");
    const char* argv[] = {"cdd", "-S", "-O", "2",
                          "-fprofile-generate=/tmp/cdd-pgo-ir", p.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    testing::internal::CaptureStdout();
    ASSERT_EQ(driver.run(), 0);
    const std::string ir = testing::internal::GetCapturedStdout();
    EXPECT_NE(ir.find("__profc_"), std::string::npos);
    std::filesystem::remove(p);
}

TEST_F(PgoDriverTest, MissingProfileUseFileFailsClearly) {
    const auto p = writeTempSource("cdd_pgo_missing.cdd", "int32 main() { return 0; }");
    const char* argv[] = {"cdd", "-S", "-O", "2",
                          "-fprofile-use=/tmp/does-not-exist.profdata", p.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    EXPECT_EQ(driver.run(), 1);
    std::filesystem::remove(p);
}
```

Add a runtime test guarded by `#ifdef CDD_TEST_PROFILE_RUNTIME` and by locating `llvm-profdata-22` or `llvm-profdata`; if either is unavailable, call `GTEST_SKIP()`. The test must:

1. compile a branchy source with `-fprofile-generate=DIR`;
2. run it with `LLVM_PROFILE_FILE=DIR/run_%p.profraw`;
3. merge with `llvm-profdata merge -o DIR/run.profdata DIR/*.profraw`;
4. compile with `-fprofile-use=DIR/run.profdata -S -O2`;
5. assert the IR contains `function_entry_count` or `branch_weights`.

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='PgoDriverTest.GenerateEmitsProfileCountersInIR:PgoDriverTest.MissingProfileUseFileFailsClearly'
```
Expected: FAIL because the pipeline does not yet add PGO passes.

- [ ] **Step 3: Implement the PGO pre/post pipeline**

In `CompilerDriver.cpp`:

- Include `llvm/Transforms/Instrumentation/PGOInstrumentation.h` and `llvm/Transforms/Instrumentation/InstrProfiling.h`.
- Before the optimization pipeline, run a small module pass manager with the relevant PGO pass:
  - `PGOInstrumentationUse(profileUseFile)` when using a profile;
  - `PGOInstrumentationGen()` when generating.
- Call the existing `runOptimizationPipeline(...)` as before; do not add a second optimization pipeline.
- After optimization and after the recursive post pass, when generating:
  ```cpp
  InstrProfOptions options;
  options.InstrProfileOutput = absoluteDir + "/default_%p.profraw";
  llvm::ModulePassManager lower;
  lower.addPass(llvm::InstrProfilingLoweringPass(options));
  lower.run(module, MAM);
  ```
  Create `absoluteDir` with `std::filesystem::absolute` and `create_directories`.
- When linking with generate mode, append `{"-u__llvm_profile_runtime", CDD_PROFILE_RUNTIME}` to `Linker::link`.
- Missing `-fprofile-use` file must return 1 with an error before object emission. The LLVM pass may diagnose; if not, check `std::filesystem::exists` first.

- [ ] **Step 4: Run focused and full tests**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='PgoDriverTest.*'
ctest --test-dir build -j8 --output-on-failure
```
Expected: PGO tests PASS or skip when the runtime is absent; full suite green.

- [ ] **Step 5: Commit**

```bash
git add src/driver/CompilerDriver.cpp tests/driver/test_pgo.cpp
git commit -m "feat: add IR PGO generate and use pipeline"
```

---

### Task 3: Profile-guided function ordering

**Files:**
- Create: `src/driver/PgoSupport.h`
- Create: `src/driver/PgoSupport.cpp`
- Modify: `src/driver/CompilerDriver.cpp`
- Test: `tests/driver/test_pgo.cpp`

**Interfaces:**
- Produces:
  ```cpp
  bool orderFunctionsByProfile(llvm::Module &module);
  ```
- Stable descending order by `Function::getEntryCount()->getCount()`; functions without counts keep their relative order after counted functions. Definitions move; declarations and aliases are not reordered as definitions.
- Called only when `profileLayout == true` and `profileUseFile` is non-empty, after the O2/post-recursion pipeline and before `emitIR`/object emission.

- [ ] **Step 1: Write the failing ordering test**

Add to `tests/driver/test_pgo.cpp`:

```cpp
TEST(PgoLayoutTest, SortsHotFunctionBeforeColdFunction) {
    // Parse a module with:
    // define i32 @cold() !prof !0 { ret i32 0 }
    // define i32 @hot()  !prof !1 { ret i32 1 }
    // !0 = !{!"function_entry_count", i64 1}
    // !1 = !{!"function_entry_count", i64 100}
    EXPECT_TRUE(orderFunctionsByProfile(module));
    EXPECT_EQ(module.getFunction("hot"), &module.getFunctionList().front());
}

TEST(PgoLayoutTest, StableWhenNoProfileCountsExist) {
    // A module with two unannotated functions must return false and keep order.
}
```

Add a driver parser/integration assertion that `-fprofile-layout` without `-fprofile-use` is accepted but does not reorder anything.

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='PgoLayoutTest.*'
```
Expected: build failure because `PgoSupport.h` and `orderFunctionsByProfile` do not exist.

- [ ] **Step 3: Implement stable function ordering**

In `PgoSupport.cpp`, collect definitions with entry counts, stable-sort by descending count, and splice the function list into that order. Preserve all uncounted functions in their original relative order after the counted ones. Return `true` only when at least two functions changed relative order.

In `CompilerDriver.cpp`, call `orderFunctionsByProfile` after the O2/post-recursion pipeline and before emitting IR/object output. Skip it when `!profileLayout || profileUseFile.empty()`.

- [ ] **Step 4: Run the focused and regression tests**

```bash
cmake --build build -j12 --target compiler_tests
./build/bin/compiler_tests --gtest_filter='PgoLayoutTest.*:PgoDriverTest.*'
ctest --test-dir build -j8 --output-on-failure
```
Expected: focused tests PASS; full suite green.

- [ ] **Step 5: Commit**

```bash
git add src/driver/PgoSupport.h src/driver/PgoSupport.cpp \
        src/driver/CompilerDriver.cpp tests/driver/test_pgo.cpp
git commit -m "feat: add opt-in profile-guided function ordering"
```

---

### Task 4: Training helper, A/B gate, and documentation

**Files:**
- Create: `scripts/pgo_train.py`
- Create: `benchmarks/results/2026-11-22-pgo-ab.md`
- Modify: `TODO.md`
- Modify: `Progress.md`

**Interfaces:**
- `scripts/pgo_train.py` CLI:
  ```text
  python3 scripts/pgo_train.py --cdd build/bin/cdd --source FILE.cdd \
      --workdir DIR [--runs 3] [--llvm-profdata PATH] [--keep]
  ```
- It builds `DIR/gen/app` with `-fprofile-generate=DIR/raw`, runs it `--runs` times with
  `LLVM_PROFILE_FILE=DIR/raw/run_%p.profraw`, merges to `DIR/app.profdata`, and
  builds `DIR/use/app` with `-fprofile-use=DIR/app.profdata -O2`.
- It prints the two binary paths and the profile path; it never edits the source.

- [ ] **Step 1: Add the helper and its smoke check**

Implement the script with `argparse`, `subprocess`, `pathlib`, and `shutil.which`.
Use `--llvm-profdata` if given, otherwise `llvm-profdata-22` then `llvm-profdata`;
error clearly if none is found.

```bash
python3 scripts/pgo_train.py --help
python3 scripts/pgo_train.py --cdd build/bin/cdd \
  --source benchmarks/cdd/bench_fib.cdd --workdir /tmp/cdd-pgo-fib --runs 1
```
Expected: help works; two binaries and a `.profdata` file are produced; both binaries print `102334155`.

- [ ] **Step 2: Run per-workload PGO A/B**

For each of `bench_class`, `bench_nqueens`, `bench_hashmap`, `bench_fib`:

1. Train with the helper.
2. Run `benchmarks/ab.py` with the baseline compiler and `--new-args="-fprofile-use=<profile>"`.
3. Record the ratio and checksum.

Do not promote PGO on a workload-by-workload basis; the opt-in workflow stays as-is.

- [ ] **Step 3: Run the layout gate**

Merge one shared profile from the training binaries and compare:

```bash
python3 benchmarks/ab.py --old-cdd build/bin/cdd \
  --new-cdd build/bin/cdd --new-args="-fprofile-use=/tmp/shared.profdata" \
  --reps 9 --warmup 5 --cpu 10 --out /tmp/pgo-shared.json

python3 benchmarks/ab.py --old-cdd build/bin/cdd \
  --new-cdd build/bin/cdd \
  --new-args="-fprofile-use=/tmp/shared.profdata -fprofile-layout" \
  --reps 9 --warmup 5 --cpu 10 --out /tmp/pgo-layout.json
```
Expected: PGO block layout may improve some workloads and regress others; the layout switch must beat the no-layout result on core/all before it is considered useful. If it does not, leave `-fprofile-layout` off and record the rejection. Do not add an LLD order-file dependency in this plan; if IR sorting fails, record the result and propose LLD as a separate measured follow-up.

- [ ] **Step 4: Full verification**

```bash
ctest --test-dir build -j8 --output-on-failure
./build/bin/cdd examples/all_features.cdd -o /tmp/all_features_pgo
/tmp/all_features_pgo | tail -1
```
Expected: full suite green; `ALL FEATURES OK`.

- [ ] **Step 5: Write the report and update progress**

Write `benchmarks/results/2026-11-22-pgo-ab.md` with:

- per-workload training/use ratios and checksums;
- shared-profile results;
- layout-on vs layout-off comparison;
- exact flags and training commands;
- the decision to keep PGO opt-in and whether `-fprofile-layout` stays off.

Add a progress snapshot to `TODO.md` and a dated section to `Progress.md`.

- [ ] **Step 6: Commit**

```bash
git add scripts/pgo_train.py benchmarks/results/2026-11-22-pgo-ab.md \
        TODO.md Progress.md
git commit -m "docs: record opt-in PGO workflow and layout results"
```
