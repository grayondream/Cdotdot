# PGO, function layout, and recursive inlining design

Date: 2026-11-22

## Intent

Close the remaining code-generation gaps after the container and link-mode work,
without sacrificing the default security posture. The two candidate areas are:

1. **PGO** as an explicit offline-training workflow, including profile-guided
   block layout and, if it measures, function ordering.
2. **Limited recursive inlining** for small self-recursive functions. GCC's
   `fib` advantage is almost entirely its recursive inlining; LLVM's inliner
   intentionally does not recurse.

The target remains `-O2`, `clang++-22`/`g++-15`, 5 warmup + 9 measured runs,
separate benchmark tiers, and checksum equality. `alloc` stays outside the core
geomean. No worktree.

## Prototype evidence

All numbers below are same-object/direct A/B with interleaved runs, not a new
absolute baseline. The host was not idle, so only the ratios are used.

### Recursive inlining prototype

A one-off LLVM pass on the already-optimised IR, limited to small self-recursive
functions and followed by a second `-O2` pipeline:

| Workload | Baseline (ms) | Recursive pass (ms) | Ratio |
|---|---:|---:|---:|
| `bench_fib` (depth 4) | 219.0 | 123.6 | **0.564** |
| `bench_nqueens` (early + late) | 386.6 | 267.6 | **0.692** |
| `bench_sort` | 461.6 | 463.1 | 1.003 |

`fib` final size was about 194 instructions; `nqueens` about 855. A full
16-workload prototype with conservative filters had core **0.922x** and all
**0.960x**, with checksums matching. The remaining regressions in that run were
mostly the unfiltered `sort` case and a second `-O2` being run for unchanged
modules; the product design below fixes both.

With the exact CDD `-O2` shape (default O2 plus the O2-only non-trivial loop
unswitch block), the product-like two-phase pipeline measured `fib`
217.0→122.0ms (**0.562x**) and `nqueens` 342.7→267.7ms (**0.781x**). The
prototype allowed one post-inline overshoot past the intended 1000-instruction
cap for `nqueens` (final 1894); the product pass will enforce the hard cap, so
the expected `nqueens` result is its pre-only shape (about 0.792x). The gate
will re-measure rather than assume the 0.781x figure.

### PGO prototype

LLVM 22 supports `pgo-instr-gen`, `pgo-instr-use`, `instrprof`, and the profile
runtime. Per-workload training and use produced mixed results:

| Workload | PGO/baseline |
|---|---:|
| `bench_class` | 0.952 |
| `bench_nqueens` | 0.979 |
| `bench_matmul` | 1.022 |
| `bench_hashmap` | 1.088 |
| `bench_fib` | ~1.11 (worse) |

Therefore PGO cannot be a default. It is useful as an opt-in workflow and needs
per-workload and shared-profile evaluation.

Function ordering was not yet promoted: `llvm-profdata` and LLD 21 are
available, but the portable first attempt will be sorting the IR function list
by profile entry count after profile use. LLD order files are a fallback only if
the IR sort fails to show a measurable gain.

## Goals

- Add a general, semantics-preserving recursive-inlining pass with strict depth
  and code-size caps.
- Make recursive inlining available at `-O2`; target default-on only after the
  full regression matrix passes.
- Add opt-in `-fprofile-generate[=DIR]` and `-fprofile-use=FILE` with an
  end-to-end training workflow.
- Use profiles for block layout first; add function ordering only if it has a
  measurable benefit.
- Keep all existing defaults safe: PGO off, `-no-pie` opt-in, no TBAA/FMA.
- Add parser/pass/e2e tests and keep all checksums unchanged.

## Non-goals

- PGO is not enabled by default and does not silently train itself.
- No new mandatory LLD dependency.
- No change to the default PIE/ASLR decision.
- No `-no-pie` default.
- No benchmark-specific source changes.

## Design

### 1. Recursive inlining

New pass: `RecursiveInliningPass` (LLVM Function/Module pass, source under
`src/driver` or the existing optimizer directory).

Rules:

- Only direct self-calls of the same function.
- Pre-O2 phase: only functions with exactly **one** direct self-call and at most
  **100 instructions**. Inline one direct self-call per iteration, up to depth
  **4**, with an inline-time function cap of **1000 instructions**.
- Post-O2 phase: targets are functions modified in the pre phase plus functions
  with exactly one self-call and at most 100 instructions. Same depth/size caps.
- The 1000-instruction cap is applied by the inliner. The conditional O2 rerun
  can still grow the function (for example, unswitching duplicated loops), so
  it is an inline-time cap, not a promise about final emitted size; measure the
  affected workloads after the rerun.
- Never touch large multi-call functions such as `bench_sort`'s quicksort unless
  they were explicitly selected by the pre phase.
- After the post phase, rerun the `-O2` pipeline **only if a function actually
  changed**. No second pipeline for unchanged modules.
- The pass verifies the module and leaves call-graph/profile metadata in a
  valid state.

CLI:

- Default: enabled at `-O2` once the full matrix passes.
- `-fno-recursive-inlining`: opt out.
- `-frecursive-inlining-depth=N`: cap (default 4).
- If compile time or code size fails the gate, the fallback is opt-in
  `-frecursive-inlining`.

### 2. PGO workflow

Flags:

- `-fprofile-generate[=DIR]`: instrument and link the profile runtime; default
  output directory is `./pgo/`, with per-process raw profiles.
- `-fprofile-use=FILE`: load a merged `.profdata` before optimization.
- Both are Linux-focused and opt-in.

Pipeline:

1. Codegen.
2. `PGOInstrumentationGen` (generate mode only).
3. Existing `-O2` pipeline, including recursive inlining.
4. `InstrProfilingLoweringPass` (generate mode only).
5. Object/link; add the compiler-rt profile archive and
   `-u__llvm_profile_runtime` only for generate mode.

For use mode, `PGOInstrumentationUse(FILE)` runs before `-O2` so block
placement, inlining, and branch layout see the profile.

Training helper: `scripts/pgo_train.py` (or `benchmarks/pgo.py`) that:

- builds instrumented binaries;
- runs a user-supplied training command or the benchmark set;
- merges raw profiles with `llvm-profdata merge`;
- rebuilds with `-fprofile-use`;
- emits a JSON/Markdown A/B report.

No driver-side silent dependency on `llvm-profdata`; the helper owns merging.

### 3. Profile-guided function layout

First implementation, portable:

- After profile use and optimization, sort the module function list by entry
  count (hot first, stable tie-break).
- Measure with and without the sort. Keep only if the all-workload and core
  results improve and no checksum changes.

Fallback, only if the IR sort loses:

- Emit function sections and use LLD `--symbol-ordering-file` or
  `--call-graph-ordering-file`; this becomes an opt-in layout mode, not a
  default dependency.

`llvm-profdata order`/temporal instrumentation is a later option; it is not in
the first milestone.

## Resolved implementation decisions

These settle the points that would otherwise make the plan guess:

- **Depth budget is per phase.** The pre phase may inline four levels; the post
  phase starts a fresh four-level budget. The inliner's 1000-instruction cap is
  checked before each inline, so a pre-modified function usually gets only one
  additional level before the cap stops it. The following O2 rerun may grow the
  function further; that growth is measured separately (`fib` 194 instructions,
  `nqueens` solve 948 instructions at default depth).
- **One call site per iteration**, chosen in IR order. The pass re-collects
  self-calls after each inline; it never expands every call site in one step.
- **`fib` is post-only.** At O0 it has two self-calls and fails the pre filter;
  after the O2 accumulator transform it has one and becomes a post target.
- **`nqueens` is pre + post.** Its `solve` starts with one self-call and is
  selected pre-O2; the post phase then continues it under the 1000-instruction
  cap. Large two-call functions such as quicksort are excluded because they are
  neither pre-selected nor small single-call post targets.
- **O2 means both pieces.** The driver exposes one `runO2Pipeline()` helper
  containing the default O2 module pipeline and the existing O2-only
  `LoopSimplify` + non-trivial `SimpleLoopUnswitch` + `InstCombine` +
  `SimplifyCFG` block. Pre-recursion runs before this helper; post-recursion
  runs after it. The conditional rerun calls the helper only and does **not**
  call the recursive pass again, which avoids an endless re-linearization loop.
- **Profile generation is before optimization; profile use is before
  optimization too.** `PGOInstrumentationGen` runs after codegen and before the
  O2 helper. `InstrProfilingLoweringPass` runs after optimization and before
  object emission. `PGOInstrumentationUse` runs before the O2 helper, so its
  counters and CFG hashes match the instrumented pre-optimization IR. Recursive
  inlining happens after profile use and therefore cannot invalidate the
  profile lookup.
- **Profile output is deterministic.** `-fprofile-generate=DIR` resolves `DIR`
  to an absolute path, creates it, and sets `InstrProfileOutput` to
  `DIR/default_%p.profraw`. `%p` prevents concurrent benchmark processes from
  overwriting each other. The helper may additionally set
  `LLVM_PROFILE_FILE`; the compiler does not depend on the environment.
- **Profile runtime policy.** CMake detects `libclang_rt.profile-*.a` through
  the configured Clang/LLVM resource directory and stores it as
  `CDD_PROFILE_RUNTIME`. If it is missing, `-fprofile-generate` fails with a
  clear error; tests that need it are skipped, not silently weakened.
- **Optimization levels.** Recursive inlining is enabled by default only at
  `-O2`; `-frecursive-inlining` can force it at `-O3`, and
  `-fno-recursive-inlining` disables it. PGO generation/use is accepted at
  `-O1`/`-O2`/`-O3`; profile-guided block layout is meaningful only when an
  optimization pipeline runs. `-fprofile-layout` is the explicit switch for
  function ordering and is off until its A/B gate passes.
- **Function ordering** runs after the O2 helper and any post-recursion rerun,
  before codegen. It uses stable descending entry counts and leaves declarations
  and aliases untouched.

## Acceptance

- `ctest` full suite green; `examples/all_features.cdd` prints
  `ALL FEATURES OK`.
- 16-workload A/B with 5 warmup + 9 measured runs; all checksums equal.
- Recursive inlining: core and all geomeans improve, with no workload regression
  outside noise; otherwise narrow the filter or make it opt-in.
- PGO: opt-in only; report per-workload and shared-profile results. Do not
  promote a profile layout that regresses the all-workload result.
- Compile-time guard: measure the corpus compile benchmark. If default recursive
  inlining causes a material compile-time regression, keep it opt-in.
- Update `TODO.md`, `Progress.md`, and `benchmarks/results/` with the direct A/B
  data and rejected variants.

## Open decisions

- Recursive inlining default-on vs opt-in: the design targets default-on at
  `-O2` after the gate, with `-fno-recursive-inlining` as the escape hatch.
- PGO function-ordering mechanism: IR sort first; LLD order files only as a
  measured fallback.
- Training methodology: per-workload profiles are an upper bound; a shared
  profile is the realistic default. Both are reported, neither becomes a
  default compiler behavior.
