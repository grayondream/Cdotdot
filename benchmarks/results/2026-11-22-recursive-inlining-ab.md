# Recursive inlining — direct A/B

Date: `2026-11-22`

This is a same-binary, flag-isolated A/B for bounded self-recursive inlining, not
a new absolute baseline. The old side is the new compiler with
`-fno-recursive-inlining`; the new side is the default `-O2` path. This keeps
the rest of the driver, standard library, linker and code layout identical to
the feature under test.

Host load was high during the run (external workloads were active), so only the
old/new ratios are meaningful. The two binaries were alternated on one pinned
CPU: 5 warmup + 9 measured rounds. All 16 checksums matched exactly.

## Result

| Tier | Benchmark | Old (ms) | New (ms) | New/old |
|---|---|---:|---:|---:|
| core | bench_fib | 302.34 | **152.18** | **0.503** |
| core | bench_sieve | 1062.86 | 1063.56 | 1.001 |
| core | bench_matmul | 44.33 | 44.59 | 1.006 |
| core | bench_nbody | 347.79 | 348.52 | 1.002 |
| core | bench_sort | 574.83 | 574.98 | 1.000 |
| core | bench_hashmap | 188.65 | 188.29 | 0.998 |
| core | bench_nqueens | 432.92 | **380.88** | **0.880** |
| core | bench_binary_search | 784.57 | 774.79 | 0.988 |
| core | bench_mandelbrot | 478.25 | 478.10 | 1.000 |
| core | bench_class | 143.67 | 141.99 | 0.988 |
| stdlib | bench_str_append | 37.83 | 37.67 | 0.996 |
| stdlib | bench_vec_push | 55.46 | 52.23 | 0.942 |
| stdlib | bench_hashmap_std | 316.62 | 317.81 | 1.004 |
| stdlib | bench_deque | 64.63 | 64.70 | 1.001 |
| alloc | bench_malloc_churn | 156.05 | 156.11 | 1.000 |
| startup | bench_empty | 1.20 | 1.20 | 0.999 |

**Core geomean: 0.9201x. All-workload geomean: 0.9458x.**

`bench_sort` is unchanged at 1.000, as intended: the quicksort has two direct
self-calls and is outside both the pre-O2 single-call filter and the post-O2
small-function filter. `fib` is post-only because its pre-O2 shape has two
self-calls; after O2 it has one and is inlined under the post cap. `nqueens`
is pre-selected and continues through the post phase under the hard cap.

The `bench_vec_push` row is measurement noise, not a code regression: compiling
that benchmark with and without `-fno-recursive-inlining` produced a
byte-identical binary (`sha256 dc507e2c...`), so the old and new sides execute
the same instructions. A separate 5+9 rerun measured 56.32ms vs 49.74ms
(0.883x), confirming that the host noise on this short workload is larger than
the reported ratio. The other sub-1% shifts are likewise within noise.

## Configuration

- Depth: 4 per phase.
- Initial selection: exactly one direct self-call and at most 100 instructions.
- Hard final cap: 1000 instructions; inline one call site per iteration.
- Pre phase runs before the O2 pipeline; post phase runs after it.
- The O2 pipeline is rerun only when the post phase changed a function; the
  recursive pass is not rerun.
- Default remains PIE; `-no-pie` is unchanged and opt-in.
- `-fno-recursive-inlining` disables the feature; `-frecursive-inlining-depth=N`
  changes the per-phase depth.

## Compile-time and code-size guard

`benchmarks/corpus/corpus_1200.cdd` (1200 synthetic functions) was compiled
with `-c -O2` and 3 warmup + 7 measured interleaved rounds:

- old: 1089.27ms median
- new: 1089.03ms median
- ratio: **0.9998x**
- object size: 295,352 bytes both sides; byte-identical object hash

No compile-time or code-size regression was observed on the corpus.

## Verification

- Full test suite: **1957/1957**.
- `examples/all_features.cdd`: `ALL FEATURES OK`.
- All 16 benchmark checksums matched.
- Raw data: `benchmarks/results/2026-11-22-recursive-inlining-ab.json`.
