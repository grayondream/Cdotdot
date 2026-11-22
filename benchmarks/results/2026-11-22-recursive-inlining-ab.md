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
| core | bench_fib | 302.35 | **151.83** | **0.502** |
| core | bench_sieve | 1045.02 | 1043.65 | 0.999 |
| core | bench_matmul | 44.67 | 45.79 | 1.025 |
| core | bench_nbody | 347.65 | 347.54 | 1.000 |
| core | bench_sort | 574.61 | 574.96 | 1.001 |
| core | bench_hashmap | 186.39 | 187.11 | 1.004 |
| core | bench_nqueens | 433.43 | **380.75** | **0.878** |
| core | bench_binary_search | 765.76 | 767.18 | 1.002 |
| core | bench_mandelbrot | 478.09 | 478.05 | 1.000 |
| core | bench_class | 139.75 | 140.58 | 1.006 |
| stdlib | bench_str_append | 38.18 | 38.00 | 0.995 |
| stdlib | bench_vec_push | 53.09 | 49.50 | 0.932 |
| stdlib | bench_hashmap_std | 315.66 | 316.95 | 1.004 |
| stdlib | bench_deque | 64.59 | 64.48 | 0.998 |
| alloc | bench_malloc_churn | 155.12 | 155.87 | 1.005 |
| startup | bench_empty | 1.21 | 1.20 | 0.989 |

**Core geomean: 0.9247x. All-workload geomean: 0.9476x.**

Only `bench_fib` and `bench_nqueens` produce different binaries between the
two sides; every other old/new artifact is byte-identical (the JSON records
both hashes). Their ratios are therefore timing noise, not code generation.

`bench_sort` is unchanged at 1.000, as intended: the quicksort has two direct
self-calls and is outside both the pre-O2 single-call filter and the post-O2
small-function filter. `fib` is post-only because its pre-O2 shape has two
self-calls; after O2 it has one and is inlined under the post cap. `nqueens`
is pre-selected and continues through the post phase under the hard cap.

The `bench_vec_push` row is measurement noise, not a code regression: compiling
that benchmark with and without `-fno-recursive-inlining` produced a
byte-identical binary (`sha256 d6b8fb27...`), so the old and new sides execute
the same instructions. A separate 5+9 rerun measured 56.32ms vs 49.74ms
(0.883x), confirming that the host noise on this short workload is larger than
the reported ratio. The same artifact-identity check holds for all 14
non-recursive benchmarks.

## Configuration

- Depth: 4 per phase.
- Initial selection: exactly one direct self-call and at most 100 instructions.
- Inline-time cap: the pass refuses the next inline when doubling the current
  body would exceed 1000 instructions; one call site per iteration. This is a
  cap on the pass output, not a promise that the following O2 rerun cannot grow
  the function (for example, non-trivial unswitching can duplicate a loop).
- Pre phase runs before the O2 pipeline; post phase runs after it.
- The O2 pipeline is rerun only when the post phase changed a function; the
  recursive pass is not rerun.
- Default remains PIE; `-no-pie` is unchanged and opt-in.
- `-fno-recursive-inlining` disables the feature; `-frecursive-inlining-depth=N`
  changes the per-phase depth.

## Emitted size after the O2 rerun

The inline-time cap is measured before the conditional O2 rerun. For the two
recursive workloads, the rerun leaves the affected functions within the 1000
instruction budget, but the object grows:

| Workload | Function | Instructions | Object (old → new) |
|---|---|---:|---:|
| bench_fib | `fib_int32` | 194 | 2,976 → 4,912 bytes |
| bench_nqueens | `solve_int32ptr_int32_int32` | 948 | 4,008 → 12,480 bytes |

`nqueens` is the expensive case: the unswitch-enabled rerun grows the object
about 3x. It is still under the inline-time instruction cap and delivers the
measured 0.880x runtime win.

## Compile-time and code-size guard

The non-recursive corpus was generated with
`python3 benchmarks/corpus/gen.py --out-dir /tmp/cdd-corpus --sizes 1200` and
compiled with `-c -O2` and 3 warmup + 7 measured interleaved rounds:

- old: 1089.27ms median
- new: 1089.03ms median
- ratio: **0.9998x**
- object size: 295,352 bytes both sides; byte-identical object hash

This corpus contains no recursive functions, so it measures only overhead on
the common path. The two workloads that actually trigger the pass were measured
separately with 5 warmup + 9 measured interleaved rounds:

| Workload | Old compile | New compile | Ratio | Object (old → new) |
|---|---:|---:|---:|---:|
| bench_fib | 21.55ms | 88.71ms | **4.12x** | 2,976 → 4,912 bytes |
| bench_nqueens | 27.94ms | 112.19ms | **4.02x** | 4,008 → 12,480 bytes |

The recursive workloads pay for the second O2 pipeline. Absolute compile cost
is still small (under 120ms for these sources), and the non-recursive corpus is
unchanged. The product decision is to keep the pass default-on at `-O2` because
the runtime wins are 0.503x / 0.880x and the user prioritized runtime; if a
large recursive program shows a material compile-time regression, the fallback
is `-fno-recursive-inlining` or making the feature opt-in.

## Verification

- Full test suite: **1959/1959**.
- `examples/all_features.cdd`: `ALL FEATURES OK`.
- All 16 benchmark checksums matched.
- Raw data: `benchmarks/results/2026-11-22-recursive-inlining-ab.json`.
