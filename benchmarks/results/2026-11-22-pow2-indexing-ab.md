# Power-of-two container indexing — same-load A/B

Date: `2026-11-22`

This is a direct old/new A/B for commit `2926a70` (Deque/HashMap/HashSet
power-of-two masks), not a new absolute baseline. The host was running an
external Java workload (~4.5 load average), so the full baseline refresh from
the same day was not promoted. The clean baseline remains
`benchmarks/results/2026-11-21-auto.json`.

## Method

- Same machine and pinned core for both binaries.
- Old: previous default auto target + O2 loop unswitch, old container code.
- New: same, with power-of-two masks.
- 5 warmup + 9 measured runs, alternating old/new order each iteration.
- Same workload, same checksum.

## Results

| Benchmark | Tier | Old (ms) | New (ms) | New/Old |
|---|---|---:|---:|---:|
| bench_fib | core | 251.15 | 251.71 | 1.002 |
| bench_sieve | core | 1320.77 | 1329.02 | 1.006 |
| bench_matmul | core | 41.49 | 40.86 | 0.985 |
| bench_nbody | core | 286.78 | 285.28 | 0.995 |
| bench_sort | core | 463.11 | 463.99 | 1.002 |
| bench_hashmap | core | 231.03 | 228.45 | 0.989 |
| bench_nqueens | core | 354.02 | 353.16 | 0.998 |
| bench_binary_search | core | 808.79 | 814.44 | 1.007 |
| bench_mandelbrot | core | 381.65 | 381.88 | 1.001 |
| bench_class | core | 211.18 | 215.63 | 1.021 |
| bench_str_append | stdlib | 49.99 | 45.10 | 0.902 |
| bench_vec_push | stdlib | 51.97 | 51.42 | 0.989 |
| bench_hashmap_std | stdlib | 440.70 | 396.21 | 0.899 |
| bench_deque | stdlib | 117.11 | 61.64 | 0.526 |
| bench_malloc_churn | alloc | 178.68 | 179.77 | 1.006 |
| bench_empty | startup | 0.99 | 1.01 | 1.024 |

- **core geomean: 1.0005x** (no core regression)
- **all 16 workloads geomean: 0.9496x**
- `deque` and `hashmap_std` are the structural wins; the small core changes are
  within run noise.

## Why

`Deque` ring indices and open-addressing probe indices used `% cap`, even
though `cap` is always zero or a power of two. The generated x86 had integer
`div` in every `push_back`/`pop`/`get`/`put`/`remove`. Replacing the modulo with
`& (cap - 1)` removes those divisions. `rehash` now rounds a caller-provided
capacity up to a power of two so the invariant holds for direct callers too.
