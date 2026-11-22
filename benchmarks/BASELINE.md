# CDD Performance Baseline — 2026-11-21-auto

> **Post-baseline delta (2026-11-22, commit `2926a70`):** `Deque`, `HashMap`,
> and `HashSet` now use power-of-two masks instead of `% cap`. A same-load
> direct A/B measured **all 0.950x**, **deque 0.526x**, and
> **hashmap_std 0.899x**, with core unchanged (1.0005x). A full baseline
> refresh was not promoted because the host was under ~4.5 load from an
> external Java process. See
> `benchmarks/results/2026-11-22-pow2-indexing-ab.md`; rerun on an idle host
> before replacing the absolute table below.

Generated: `2026-11-21T23:02:49`
Command: `/home/ares/apps/conda/bin/python3 benchmarks/run.py --full --baseline-md --label 2026-11-21-auto`

## Environment

| Item | Value |
|---|---|
| CPU | AMD Ryzen 5 5600X 6-Core Processor |
| CPU count / pinned | 12 / 0 |
| Governor | powersave |
| Load average (1/5/15 min) | 1.81 / 1.48 / 1.57 |
| Load average at end | 1.65 / 1.54 / 1.55 |
| Selected core busy at start | 0.0% |
| Kernel | 7.0.0-43-generic (x86_64) |
| CDD | `/media/nvm1/workspace/Cdotdot/build/bin/cdd` (3353288 bytes, build type `Release`) |
| CDD commit | `8c92223` master |
| LLVM (llvm-config) | 22.1.2 |
| clang | Ubuntu clang version 22.1.2 (1ubuntu1) |
| gcc | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |

Method: `-O2` for every compiler, 5 warmup + 9 measured runs (median) for runtime, 1 warmup + 5 runs for compile time, one implementation round at a time (interleaved), process pinned with `taskset`. Each row runs the same algorithm, data types and workload size in CDD and C++. C++ references use `-g` because CDD emits debug information by default. The report also records peak RSS, LLVM IR size, object-section breakdown, stripped executable size, and an optional `-O0..-O3` matrix.

## Headline

- `core` geometric mean: **0.82x** vs clang, **0.89x** vs gcc
- `stdlib` geometric mean: **0.32x** vs clang, **0.35x** vs gcc
- `alloc` geometric mean: **0.31x** vs clang, **0.30x** vs gcc
- `startup` geometric mean: **0.68x** vs clang, **1.00x** vs gcc
- Largest core gap vs clang: `bench_fib` at **1.05x**; largest core win: `bench_matmul` at **0.53x**.

## Runtime (median, lower is better; ratio = CDD / reference)

| Benchmark | Tier | CDD (ms) | clang (ms) | gcc (ms) | CDD/clang | CDD/gcc | checksum |
|---|---|---|---|---|---|---|---|
| bench_fib | core | 257.0 | 244.1 | 137.7 | 1.05x | 1.87x | 102334155 |
| bench_sieve | core | 1045.1 | 1203.2 | 1198.6 | 0.87x | 0.87x | 11078937 |
| bench_matmul | core | 37.4 | 71.1 | 72.9 | 0.53x | 0.51x | 2876 |
| bench_nbody | core | 277.3 | 294.0 | 293.5 | 0.94x | 0.94x | 1690136770432667 |
| bench_sort | core | 459.9 | 447.4 | 453.6 | 1.03x | 1.01x | 8588201207136256 |
| bench_hashmap | core | 183.6 | 301.6 | 285.2 | 0.61x | 0.64x | 4000000 |
| bench_nqueens | core | 344.7 | 412.7 | 344.6 | 0.84x | 1.00x | 73712 |
| bench_binary_search | core | 681.9 | 662.0 | 661.8 | 1.03x | 1.03x | 8382020676000 |
| bench_mandelbrot | core | 382.4 | 382.4 | 381.2 | 1.00x | 1.00x | 172812923 |
| bench_class | core | 136.7 | 231.3 | 224.9 | 0.59x | 0.61x | 67884192 |
| bench_str_append | stdlib | 35.1 | 241.5 | 239.2 | 0.15x | 0.15x | 8976000000 |
| bench_vec_push | stdlib | 49.9 | 144.5 | 142.4 | 0.35x | 0.35x | 1535999952000000 |
| bench_hashmap_std | stdlib | 323.1 | 1562.7 | 1415.5 | 0.21x | 0.23x | 3999998000000 |
| bench_deque | stdlib | 115.5 | 108.4 | 94.1 | 1.07x | 1.23x | 511999984000000 |
| bench_malloc_churn | alloc | 156.6 | 505.2 | 517.5 | 0.31x | 0.30x | 1524000000 |
| bench_empty | startup | 1.0 | 1.5 | 1.0 | 0.68x | 1.00x |  |

### Geometric mean by tier

| Tier | CDD/clang | CDD/gcc |
|---|---|---|
| core (10) | 0.82x | 0.89x |
| stdlib (4) | 0.32x | 0.35x |
| alloc (1) | 0.31x | 0.30x |
| startup (1) | 0.68x | 1.00x |

### Run-to-run stability (CV %, lower is better)

| Benchmark | CDD CV | clang CV | gcc CV |
|---|---|---|---|
| bench_fib | 4.0% | 3.4% | 7.2% |
| bench_sieve | 0.5% | 0.8% | 0.2% |
| bench_matmul | 2.5% | 1.1% | 5.1% |
| bench_nbody | 0.2% | 0.2% | 0.2% |
| bench_sort | 0.2% | 0.2% | 0.3% |
| bench_hashmap | 2.1% | 3.6% | 3.1% |
| bench_nqueens | 2.2% | 1.5% | 0.9% |
| bench_binary_search | 1.5% | 3.7% | 3.0% |
| bench_mandelbrot | 0.5% | 0.9% | 0.1% |
| bench_class | 4.1% | 3.4% | 4.0% |
| bench_str_append | 14.5% | 1.0% | 0.9% |
| bench_vec_push | 20.5% | 6.0% | 10.6% |
| bench_hashmap_std | 1.5% | 0.7% | 0.8% |
| bench_deque | 0.6% | 0.8% | 0.8% |
| bench_malloc_churn | 2.0% | 1.2% | 0.2% |
| bench_empty | 3.4% | 1.8% | 2.8% |

## Optimization-level matrix (runtime median ms)

2 warmup + 3 runs per cell. Rows are the same workload compiled with a different `-O` level.

| Benchmark | O | CDD | clang | gcc | CDD/clang | CDD/gcc |
|---|---|---|---|---|---|---|
| bench_fib | O0 | 374.2 | 465.0 | 414.8 | 0.80x | 0.90x |
| bench_fib | O1 | 186.4 | 222.9 | 340.4 | 0.84x | 0.55x |
| bench_fib | O2 | 244.9 | 231.2 | 127.7 | 1.06x | 1.92x |
| bench_fib | O3 | 244.4 | 231.3 | 124.2 | 1.06x | 1.97x |
| bench_matmul | O0 | 621.1 | 792.4 | 845.9 | 0.78x | 0.73x |
| bench_matmul | O1 | 39.4 | 121.9 | 131.3 | 0.32x | 0.30x |
| bench_matmul | O2 | 36.9 | 71.3 | 72.8 | 0.52x | 0.51x |
| bench_matmul | O3 | 37.6 | 70.8 | 65.4 | 0.53x | 0.57x |
| bench_nbody | O0 | 605.2 | 1136.7 | 1351.4 | 0.53x | 0.45x |
| bench_nbody | O1 | 281.9 | 276.8 | 279.6 | 1.02x | 1.01x |
| bench_nbody | O2 | 277.0 | 294.3 | 293.2 | 0.94x | 0.94x |
| bench_nbody | O3 | 292.7 | 281.6 | 303.4 | 1.04x | 0.96x |
| bench_hashmap | O0 | 497.2 | 641.7 | 563.1 | 0.77x | 0.88x |
| bench_hashmap | O1 | 185.8 | 295.2 | 325.7 | 0.63x | 0.57x |
| bench_hashmap | O2 | 183.9 | 302.7 | 280.2 | 0.61x | 0.66x |
| bench_hashmap | O3 | 182.7 | 294.2 | 279.9 | 0.62x | 0.65x |
| bench_nqueens | O0 | 817.1 | 926.6 | 861.0 | 0.88x | 0.95x |
| bench_nqueens | O1 | 421.0 | 422.9 | 362.2 | 1.00x | 1.16x |
| bench_nqueens | O2 | 353.8 | 415.7 | 349.0 | 0.85x | 1.01x |
| bench_nqueens | O3 | 343.9 | 300.0 | 333.6 | 1.15x | 1.03x |
| bench_str_append | O0 | 153.7 | 2449.1 | 2196.9 | 0.06x | 0.07x |
| bench_str_append | O1 | 36.3 | 257.8 | 253.8 | 0.14x | 0.14x |
| bench_str_append | O2 | 34.7 | 242.7 | 245.0 | 0.14x | 0.14x |
| bench_str_append | O3 | 40.9 | 242.8 | 246.8 | 0.17x | 0.17x |

This matrix is diagnostic: it shows whether a gap is already present at `-O0` (front-end/codegen shape) or only appears after optimization.

## Compile time (median ms, lower is better)

| Source | CDD `-c` | CDD syntax | clang `-c` | gcc `-c` | clang syntax | gcc syntax |
|---|---|---|---|---|---|---|
| bench_fib | 19.2 | 12.5 | 31.8 | 43.7 | 23.4 | 10.7 |
| bench_sieve | 25.2 | 13.3 | 43.1 | 35.7 | 29.4 | 14.9 |
| bench_matmul | 25.8 | 12.9 | 44.3 | 40.2 | 29.2 | 15.0 |
| bench_nbody | 33.6 | 13.7 | 179.2 | 144.0 | 153.0 | 94.2 |
| bench_sort | 35.0 | 13.3 | 53.0 | 49.6 | 29.9 | 15.0 |
| bench_hashmap | 23.7 | 12.9 | 42.9 | 42.0 | 30.0 | 15.2 |
| bench_nqueens | 24.6 | 12.8 | 42.9 | 36.1 | 29.7 | 14.8 |
| bench_binary_search | 24.0 | 13.1 | 43.1 | 35.9 | 29.7 | 15.0 |
| bench_mandelbrot | 22.5 | 13.3 | 35.9 | 28.8 | 24.2 | 11.7 |
| bench_class | 33.1 | 13.5 | 46.8 | 40.0 | 29.7 | 14.8 |
| bench_str_append | 23.7 | 12.7 | 188.6 | 151.1 | 161.4 | 105.5 |
| bench_vec_push | 63.1 | 15.8 | 161.8 | 123.0 | 130.1 | 79.7 |
| bench_hashmap_std | 91.5 | 17.7 | 220.7 | 181.6 | 161.4 | 101.4 |
| bench_deque | 61.1 | 15.5 | 167.0 | 136.5 | 116.5 | 71.3 |
| bench_malloc_churn | 22.0 | 13.5 | 42.1 | 35.6 | 30.2 | 15.6 |
| bench_empty | 16.7 | 12.8 | 22.6 | 13.8 | 17.9 | 6.1 |
| corpus_100 | 94.1 | 17.3 | 129.9 | 202.4 | 26.5 | 10.0 |
| corpus_300 | 230.4 | 22.3 | 335.2 | 557.5 | 35.5 | 14.8 |
| corpus_600 | 450.7 | 33.9 | 685.3 | 1148.6 | 52.3 | 23.0 |
| corpus_1200 | 873.4 | 54.7 | 1436.7 | 2392.5 | 83.2 | 38.7 |

`syntax` is CDD's `-fsyntax-only` (lexer/parser/imports, no semantic analysis) versus the reference's `-fsyntax-only` (which does include semantic analysis), so the syntax columns are diagnostic, not a fair cross-language ratio.

### Compile scaling (synthetic corpus)

| Functions | CDD `-c` | clang `-c` | gcc `-c` | CDD ms/fn | clang ms/fn | gcc ms/fn |
|---|---|---|---|---|---|
| 100 | 94.1 | 129.9 | 202.4 | 0.94 | 1.30 | 2.02 |
| 300 | 230.4 | 335.2 | 557.5 | 0.77 | 1.12 | 1.86 |
| 600 | 450.7 | 685.3 | 1148.6 | 0.75 | 1.14 | 1.91 |
| 1200 | 873.4 | 1436.7 | 2392.5 | 0.73 | 1.20 | 1.99 |

`ms/fn` normalizes the fixed compiler startup cost and shows the marginal cost of one generated function.

## Code size: object sections (KiB)

| Benchmark | CDD `.text` | clang `.text` | gcc `.text` | CDD `.rodata` | CDD `.data` | CDD `.bss` | CDD debug |
|---|---|---|---|---|---|---|---|
| bench_fib | 0.1 | 0.1 | 0.9 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_sieve | 0.4 | 0.3 | 0.3 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_matmul | 0.8 | 0.6 | 0.7 | 0.1 | 0.0 | 0.0 | 0.5 |
| bench_nbody | 1.9 | 1.3 | 1.5 | 0.1 | 0.0 | 0.0 | 0.8 |
| bench_sort | 1.1 | 0.7 | 0.7 | 0.0 | 0.0 | 0.0 | 1.0 |
| bench_hashmap | 0.4 | 0.3 | 0.5 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_nqueens | 0.3 | 0.2 | 0.2 | 0.0 | 0.0 | 0.0 | 0.8 |
| bench_binary_search | 0.5 | 0.3 | 0.2 | 0.1 | 0.0 | 0.0 | 0.5 |
| bench_mandelbrot | 0.3 | 0.3 | 0.3 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_class | 1.6 | 0.4 | 0.4 | 0.2 | 0.0 | 0.0 | 0.6 |
| bench_str_append | 0.3 | 0.4 | 0.6 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_vec_push | 2.5 | 0.4 | 0.5 | 0.0 | 0.0 | 0.0 | 3.4 |
| bench_hashmap_std | 5.2 | 1.1 | 1.3 | 0.0 | 0.0 | 0.0 | 4.7 |
| bench_deque | 2.8 | 1.5 | 1.5 | 0.0 | 0.0 | 0.0 | 3.4 |
| bench_malloc_churn | 0.2 | 0.2 | 0.2 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_empty | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | 0.4 |

Object `.text` is generated code. `.rodata`/`.data`/`.bss` show static data; `.debug` shows debug-info payload. Debug sections are not part of the loaded code size.

## Code size: executables (KiB)

| Benchmark | CDD | CDD stripped | clang | clang stripped | gcc | gcc stripped | CDD/clang |
|---|---|---|---|---|---|---|---|
| bench_fib | 16.6 | 14.1 | 21.9 | 14.2 | 22.8 | 14.1 | 0.76x |
| bench_sieve | 259.0 | 222.3 | 24.5 | 14.2 | 24.4 | 14.1 | 10.59x |
| bench_matmul | 259.0 | 222.3 | 24.6 | 14.2 | 24.4 | 14.1 | 10.52x |
| bench_nbody | 259.5 | 222.3 | 31.7 | 14.2 | 28.1 | 14.1 | 8.20x |
| bench_sort | 259.7 | 222.3 | 25.7 | 14.2 | 25.8 | 14.1 | 10.09x |
| bench_hashmap | 259.0 | 222.3 | 25.2 | 14.2 | 25.4 | 14.1 | 10.28x |
| bench_nqueens | 259.5 | 222.3 | 25.1 | 14.2 | 24.8 | 14.1 | 10.33x |
| bench_binary_search | 259.0 | 222.3 | 24.7 | 14.2 | 24.4 | 14.1 | 10.49x |
| bench_mandelbrot | 16.5 | 14.1 | 22.5 | 14.2 | 22.4 | 14.1 | 0.73x |
| bench_class | 259.2 | 222.3 | 25.6 | 14.2 | 25.2 | 14.1 | 10.14x |
| bench_str_append | 259.0 | 222.3 | 60.0 | 14.3 | 65.4 | 14.2 | 4.32x |
| bench_vec_push | 268.7 | 226.3 | 43.8 | 14.3 | 53.6 | 14.2 | 6.14x |
| bench_hashmap_std | 274.8 | 230.3 | 106.7 | 14.3 | 127.0 | 14.2 | 2.57x |
| bench_deque | 268.6 | 226.3 | 53.2 | 14.3 | 68.2 | 14.2 | 5.05x |
| bench_malloc_churn | 255.1 | 218.3 | 24.5 | 14.2 | 24.3 | 14.1 | 10.41x |
| bench_empty | 16.4 | 14.0 | 16.5 | 14.1 | 16.6 | 14.0 | 0.99x |

CDD links mimalloc statically, so its executables are much larger than the C++ references even when generated code is comparable. `stripped` removes symbols/debug information from a copy.

## LLVM IR size (diagnostic)

| Source | CDD bytes | CDD lines | clang bytes | clang lines | CDD/clang bytes |
|---|---|---|---|---|---|
| bench_fib | 2598 | 55 | 38693 | 427 | 0.07x |
| bench_sieve | 9104 | 165 | 63323 | 738 | 0.14x |
| bench_matmul | 10708 | 183 | 67026 | 789 | 0.16x |
| bench_nbody | 29049 | 446 | 137391 | 1553 | 0.21x |
| bench_sort | 28397 | 499 | 79927 | 1021 | 0.36x |
| bench_hashmap | 9446 | 168 | 69092 | 836 | 0.14x |
| bench_nqueens | 8829 | 165 | 64928 | 748 | 0.14x |
| bench_binary_search | 8209 | 150 | 63087 | 737 | 0.13x |
| bench_mandelbrot | 6867 | 131 | 46184 | 560 | 0.15x |
| bench_class | 25813 | 416 | 74112 | 877 | 0.35x |
| bench_str_append | 8524 | 143 | 222822 | 2166 | 0.04x |
| bench_vec_push | 65427 | 1221 | 161109 | 1704 | 0.41x |
| bench_hashmap_std | 112046 | 2026 | 409521 | 3952 | 0.27x |
| bench_deque | 64505 | 1193 | 250127 | 2910 | 0.26x |
| bench_malloc_churn | 4517 | 84 | 61282 | 705 | 0.07x |
| bench_empty | 1034 | 22 | 2317 | 36 | 0.45x |
| corpus_100 | 50771 | 1023 | 378316 | 5043 | 0.13x |
| corpus_300 | 152099 | 3023 | 1147139 | 15043 | 0.13x |
| corpus_600 | 304925 | 6023 | 2300952 | 30043 | 0.13x |
| corpus_1200 | 613926 | 12023 | 4650481 | 60043 | 0.13x |

Both are LLVM IR, but front ends attach different metadata and debug information; use this as a diagnostic for CDD's own IR growth, not as a strict cross-language size ratio.

## Peak memory: runtime (MiB)

| Benchmark | CDD | clang | gcc | CDD/clang |
|---|---|---|---|---|
| bench_fib | 1.7 | 4.1 | 1.7 | 0.41x |
| bench_sieve | 196.0 | 194.6 | 192.4 | 1.01x |
| bench_matmul | 18.0 | 17.3 | 15.2 | 1.04x |
| bench_nbody | 4.1 | 4.1 | 1.7 | 1.00x |
| bench_sort | 34.0 | 34.3 | 32.2 | 0.99x |
| bench_hashmap | 166.0 | 162.3 | 160.3 | 1.02x |
| bench_nqueens | 4.0 | 4.1 | 1.7 | 0.97x |
| bench_binary_search | 70.0 | 67.8 | 65.7 | 1.03x |
| bench_mandelbrot | 1.7 | 4.1 | 1.7 | 0.41x |
| bench_class | 50.0 | 49.4 | 47.5 | 1.01x |
| bench_str_append | 185.8 | 243.6 | 243.7 | 0.76x |
| bench_vec_push | 181.8 | 131.6 | 131.7 | 1.38x |
| bench_hashmap_std | 166.1 | 186.7 | 186.7 | 0.89x |
| bench_deque | 181.7 | 131.7 | 131.7 | 1.38x |
| bench_malloc_churn | 418.1 | 507.3 | 505.3 | 0.82x |
| bench_empty | 1.4 | 3.8 | 1.4 | 0.37x |

Peak RSS is measured once per implementation and includes the static mimalloc arena for CDD; allocator caching can make it sensitive to run order.

### Peak memory: compile (MiB)

| Source | CDD | clang | gcc |
|---|---|---|---|
| bench_fib | 86.2 | 104.6 | 31.4 |
| bench_sieve | 88.1 | 106.5 | 32.5 |
| bench_matmul | 88.3 | 106.8 | 34.8 |
| bench_nbody | 93.5 | 120.5 | 64.2 |
| bench_sort | 90.0 | 107.3 | 33.9 |
| bench_hashmap | 86.7 | 105.8 | 32.5 |
| bench_nqueens | 86.6 | 106.4 | 31.8 |
| bench_binary_search | 87.7 | 106.6 | 32.3 |
| bench_mandelbrot | 88.9 | 105.2 | 30.7 |
| bench_class | 94.8 | 107.5 | 33.8 |
| bench_str_append | 90.0 | 121.7 | 67.9 |
| bench_vec_push | 94.6 | 118.8 | 56.3 |
| bench_hashmap_std | 97.6 | 122.8 | 68.4 |
| bench_deque | 97.6 | 118.2 | 57.6 |
| bench_malloc_churn | 86.6 | 105.7 | 31.6 |
| bench_empty | 81.4 | 96.4 | 23.4 |
| corpus_100 | 91.4 | 104.0 | 35.2 |
| corpus_300 | 105.5 | 111.6 | 52.6 |
| corpus_600 | 125.4 | 123.0 | 75.0 |
| corpus_1200 | 157.5 | 146.5 | 121.4 |

## Hardware counters (one representative run)

| Benchmark | Impl | cycles (M) | instructions (M) | IPC | GHz | cache-miss % | branch-miss % |
|---|---|---|---|---|---|---|---|
| bench_fib | cdd | 1115.4 | 3648.4 | 3.27 | 4.64 | 0.00 | 0.21 |
| bench_fib | clang | 1055.4 | 3650.7 | 3.46 | 4.63 | 0.00 | 0.21 |
| bench_fib | gcc | 576.7 | 2763.1 | 4.79 | 4.64 | 0.00 | 0.42 |
| bench_sieve | cdd | 4838.5 | 2076.0 | 0.43 | 4.64 | 6.56 | 0.15 |
| bench_sieve | clang | 5583.1 | 2528.9 | 0.45 | 4.64 | 5.85 | 1.22 |
| bench_sieve | gcc | 5572.7 | 2501.8 | 0.45 | 4.65 | 5.93 | 1.22 |
| bench_matmul | cdd | 161.1 | 393.4 | 2.44 | 4.48 | 0.38 | 0.38 |
| bench_matmul | clang | 318.9 | 1343.4 | 4.21 | 4.60 | 0.15 | 0.85 |
| bench_matmul | gcc | 329.7 | 1849.8 | 5.61 | 4.60 | 0.11 | 0.47 |
| bench_nbody | cdd | 1282.4 | 2581.7 | 2.01 | 4.65 | 0.00 | 0.13 |
| bench_nbody | clang | 1363.2 | 3014.1 | 2.21 | 4.64 | 0.00 | 0.16 |
| bench_nbody | gcc | 1355.7 | 2951.4 | 2.18 | 4.64 | 0.00 | 0.20 |
| bench_sort | cdd | 2134.0 | 2018.4 | 0.95 | 4.64 | 0.04 | 18.83 |
| bench_sort | clang | 2074.4 | 1959.6 | 0.94 | 4.65 | 0.06 | 20.10 |
| bench_sort | gcc | 2106.4 | 2189.8 | 1.04 | 4.64 | 0.05 | 20.03 |
| bench_hashmap | cdd | 846.0 | 311.4 | 0.37 | 4.64 | 9.45 | 0.35 |
| bench_hashmap | clang | 1332.6 | 584.1 | 0.44 | 4.65 | 6.73 | 4.26 |
| bench_hashmap | gcc | 1279.3 | 543.5 | 0.42 | 4.65 | 7.23 | 4.88 |
| bench_nqueens | cdd | 1594.1 | 3929.0 | 2.46 | 4.64 | 0.00 | 3.11 |
| bench_nqueens | clang | 1945.1 | 4027.1 | 2.07 | 4.64 | 0.00 | 2.95 |
| bench_nqueens | gcc | 1595.9 | 4126.7 | 2.59 | 4.64 | 0.00 | 2.91 |
| bench_binary_search | cdd | 3195.9 | 557.4 | 0.17 | 4.64 | 36.22 | 17.50 |
| bench_binary_search | clang | 3107.0 | 663.0 | 0.21 | 4.65 | 18.39 | 16.54 |
| bench_binary_search | gcc | 2983.8 | 576.6 | 0.19 | 4.65 | 20.17 | 16.37 |
| bench_mandelbrot | cdd | 1786.3 | 1883.9 | 1.05 | 4.64 | 0.01 | 0.21 |
| bench_mandelbrot | clang | 1770.5 | 2709.5 | 1.53 | 4.64 | 0.00 | 0.18 |
| bench_mandelbrot | gcc | 1766.6 | 2787.1 | 1.58 | 4.64 | 0.00 | 0.13 |
| bench_class | cdd | 628.0 | 489.0 | 0.78 | 4.63 | 0.23 | 0.39 |
| bench_class | clang | 1054.1 | 3249.0 | 3.08 | 4.64 | 0.05 | 0.60 |
| bench_class | gcc | 1037.6 | 4249.0 | 4.10 | 4.63 | 0.04 | 0.59 |
| bench_str_append | cdd | 202.7 | 231.9 | 1.14 | 4.61 | 0.35 | 0.77 |
| bench_str_append | clang | 1101.8 | 1303.5 | 1.18 | 4.64 | 0.42 | 5.01 |
| bench_str_append | gcc | 1101.9 | 1409.6 | 1.28 | 4.64 | 0.40 | 5.19 |
| bench_vec_push | cdd | 271.2 | 464.7 | 1.71 | 4.64 | 0.18 | 0.27 |
| bench_vec_push | clang | 665.7 | 710.6 | 1.07 | 4.64 | 0.48 | 5.56 |
| bench_vec_push | gcc | 643.0 | 766.3 | 1.19 | 4.64 | 0.45 | 4.78 |
| bench_hashmap_std | cdd | 1552.7 | 1318.9 | 0.85 | 4.64 | 1.78 | 4.10 |
| bench_hashmap_std | clang | 7243.5 | 2459.2 | 0.34 | 4.65 | 2.58 | 3.23 |
| bench_hashmap_std | gcc | 6895.7 | 2359.8 | 0.34 | 4.65 | 2.65 | 3.40 |
| bench_deque | cdd | 528.8 | 1188.6 | 2.25 | 4.64 | 0.05 | 0.08 |
| bench_deque | clang | 500.0 | 1041.8 | 2.08 | 4.64 | 0.17 | 2.27 |
| bench_deque | gcc | 431.0 | 845.5 | 1.96 | 4.64 | 0.21 | 2.64 |
| bench_malloc_churn | cdd | 718.9 | 544.4 | 0.76 | 4.64 | 4.73 | 0.51 |
| bench_malloc_churn | clang | 2157.7 | 2834.5 | 1.31 | 4.65 | 1.58 | 2.42 |
| bench_malloc_churn | gcc | 2260.2 | 2855.4 | 1.26 | 4.64 | 1.53 | 2.42 |
| bench_empty | cdd | 1.1 | 0.8 | 0.72 | 3.24 | 2.95 | 12.37 |
| bench_empty | clang | 2.8 | 3.2 | 1.12 | 3.91 | 1.51 | 7.54 |
| bench_empty | gcc | 1.1 | 0.8 | 0.72 | 3.30 | 3.29 | 12.40 |

## Analysis and optimization targets

- Largest core gaps vs clang: `bench_fib` 1.05x, `bench_binary_search` 1.03x, `bench_sort` 1.03x.
- Largest core wins vs clang: `bench_hashmap` 0.61x, `bench_class` 0.59x, `bench_matmul` 0.53x.
- With the auto x86-64-v3 target, matmul, class, and hashmap are now the largest core wins. The remaining core gaps (fib, sort, binary_search) are close to clang at the code-generation level; inspect allocator/layout and GCC's recursive-inlining shape before adding more IR metadata.
- `bench_fib` is close to clang but well behind gcc. GCC emits a different recursive shape/inlining decision; treat this as a codegen-shape question, not a front-end question.
- `bench_nqueens`, `bench_binary_search`, `bench_mandelbrot`, and `bench_class` extend the core coverage to backtracking, branchy memory access, floating-point control flow, and by-value method calls. They make it harder to improve the core geomean by tuning one hot loop.
- `stdlib` gaps reflect both generated code and the CDD implementation (`string`, `Vec`, `HashMap`, `Deque`). Keep them separate from core when judging the compiler backend.
- `alloc` is dominated by mimalloc versus the platform allocator; `startup` is process overhead. Neither should drive backend optimization decisions.
- Highest-value follow-ups: binary_search memory layout/allocator behavior, sort branch layout, and the `std::Deque` growth path. fib is dominated by GCC's recursive inlining, not missing CDD IR metadata. Use `compare.py` to quantify each change against this baseline.

## Caveats

- The CDD compiler uses LLVM 22.1.2; `clang++` may use a different LLVM release. The `g++` column is an independent GCC reference.
- The main tables use `-O2`; the optimization-level matrix covers `-O0..-O3`. No fast-math. The CDD default target is `auto`: `x86-64-v3` on AVX2 hosts, `generic` otherwise; pass `-march=generic` for the portable SSE2 baseline.
- The CPU governor is read live; unless it is `performance`, boost behaviour adds run-to-run noise. Medians over the measured runs and CPU pinning keep this small but not zero; the CV table makes the spread explicit.
- This baseline may be taken on a shared desktop (`load_average` start and end are recorded above). The runner auto-selects the least busy core and interleaves implementations to keep the comparison fair, but an idle machine will produce lower absolute times. Ratios are the intended comparison metric.
- Runtime includes process start and exit (typically 1-3 ms); the `startup` tier measures that overhead directly.
- The `alloc` tier compares CDD's static mimalloc against the C++ reference's system allocator; read it as a product-level number, not an allocator-to-allocator contest. It is excluded from the core geomean.
- `stdlib` results depend on both code generation and the standard library implementation. `std::HashMap`/`std::unordered_map` and `std::Deque`/`std::deque` have different growth and bucket strategies; these rows are product-level comparisons.
- Compile time on the small benchmark sources is dominated by process startup and the automatic std prelude; use the `corpus_*` rows and their `ms/fn` columns for throughput comparisons.
- LLVM IR size is a diagnostic for CDD's own optimizer growth. The two front ends attach different metadata/debug information, so the IR ratio is not a strict cross-language code-size metric.
- Peak RSS is sampled once per implementation; allocator caching and the static mimalloc arena make it sensitive to run order.
- `startup` hardware counters are dominated by process setup and are only informative for the wall-clock column.
