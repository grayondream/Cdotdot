# CDD Performance Baseline — 2026-11-18-fixed

Generated: `2026-11-18T22:25:18`
Command: `/home/ares/apps/conda/bin/python3 benchmarks/run.py --full --baseline-md --label 2026-11-18-fixed`

## Environment

| Item | Value |
|---|---|
| CPU | AMD Ryzen 5 5600X 6-Core Processor |
| CPU count / pinned | 12 / 3 |
| Governor | powersave |
| Load average (1/5/15 min) | 6.94 / 6.36 / 4.32 |
| Load average at end | 2.34 / 3.06 / 3.47 |
| Selected core busy at start | 0.0% |
| Kernel | 7.0.0-43-generic (x86_64) |
| CDD | `/media/nvm1/workspace/Cdotdot/build/bin/cdd` (3300160 bytes, build type `Release`) |
| CDD commit | `c79cd5f` master (dirty worktree) |
| LLVM (llvm-config) | 21.1.8 |
| clang | Ubuntu clang version 22.1.2 (1ubuntu1) |
| gcc | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |

Method: `-O2` for every compiler, 5 warmup + 9 measured runs (median) for runtime, 1 warmup + 5 runs for compile time, one implementation round at a time (interleaved), process pinned with `taskset`. Each row runs the same algorithm, data types and workload size in CDD and C++. C++ references use `-g` because CDD emits debug information by default. The report also records peak RSS, LLVM IR size, object-section breakdown, stripped executable size, and an optional `-O0..-O3` matrix.

## Headline

- `core` geometric mean: **1.06x** vs clang, **1.15x** vs gcc
- `stdlib` geometric mean: **0.39x** vs clang, **0.41x** vs gcc
- `alloc` geometric mean: **0.30x** vs clang, **0.30x** vs gcc
- `startup` geometric mean: **0.71x** vs clang, **1.01x** vs gcc
- Largest core gap vs clang: `bench_matmul` at **2.91x**; largest core win: `bench_hashmap` at **0.70x**.

## Runtime (median, lower is better; ratio = CDD / reference)

| Benchmark | Tier | CDD (ms) | clang (ms) | gcc (ms) | CDD/clang | CDD/gcc | checksum |
|---|---|---|---|---|---|---|---|
| bench_fib | core | 242.0 | 228.1 | 125.1 | 1.06x | 1.93x | 102334155 |
| bench_sieve | core | 1085.7 | 1214.8 | 1214.0 | 0.89x | 0.89x | 11078937 |
| bench_matmul | core | 204.3 | 70.2 | 71.4 | 2.91x | 2.86x | 2876 |
| bench_nbody | core | 292.5 | 294.1 | 293.0 | 0.99x | 1.00x | 1690136770432667 |
| bench_sort | core | 462.7 | 448.0 | 453.6 | 1.03x | 1.02x | 8588201207136256 |
| bench_hashmap | core | 207.8 | 294.9 | 293.1 | 0.70x | 0.71x | 4000000 |
| bench_nqueens | core | 335.2 | 426.1 | 345.6 | 0.79x | 0.97x | 73712 |
| bench_binary_search | core | 684.8 | 687.2 | 668.4 | 1.00x | 1.02x | 8382020676000 |
| bench_mandelbrot | core | 378.5 | 379.4 | 378.2 | 1.00x | 1.00x | 172812923 |
| bench_class | core | 259.6 | 229.2 | 229.3 | 1.13x | 1.13x | 67884192 |
| bench_str_append | stdlib | 58.6 | 244.0 | 243.7 | 0.24x | 0.24x | 8976000000 |
| bench_vec_push | stdlib | 53.7 | 137.1 | 137.6 | 0.39x | 0.39x | 1535999952000000 |
| bench_hashmap_std | stdlib | 335.3 | 1585.2 | 1419.3 | 0.21x | 0.24x | 3999998000000 |
| bench_deque | stdlib | 127.1 | 108.2 | 95.0 | 1.17x | 1.34x | 511999984000000 |
| bench_malloc_churn | alloc | 152.9 | 517.6 | 504.4 | 0.30x | 0.30x | 1524000000 |
| bench_empty | startup | 1.0 | 1.4 | 1.0 | 0.71x | 1.01x |  |

### Geometric mean by tier

| Tier | CDD/clang | CDD/gcc |
|---|---|---|
| core (10) | 1.06x | 1.15x |
| stdlib (4) | 0.39x | 0.41x |
| alloc (1) | 0.30x | 0.30x |
| startup (1) | 0.71x | 1.01x |

### Run-to-run stability (CV %, lower is better)

| Benchmark | CDD CV | clang CV | gcc CV |
|---|---|---|---|
| bench_fib | 2.1% | 2.5% | 3.9% |
| bench_sieve | 0.4% | 0.3% | 0.8% |
| bench_matmul | 1.0% | 4.3% | 2.1% |
| bench_nbody | 28.4% | 11.3% | 9.6% |
| bench_sort | 0.6% | 0.9% | 0.6% |
| bench_hashmap | 2.8% | 2.9% | 1.4% |
| bench_nqueens | 2.3% | 1.1% | 1.5% |
| bench_binary_search | 0.9% | 1.0% | 1.2% |
| bench_mandelbrot | 0.3% | 0.1% | 0.0% |
| bench_class | 1.3% | 3.2% | 2.1% |
| bench_str_append | 8.4% | 1.4% | 1.1% |
| bench_vec_push | 7.4% | 2.5% | 1.0% |
| bench_hashmap_std | 1.7% | 1.3% | 0.9% |
| bench_deque | 0.2% | 0.5% | 0.3% |
| bench_malloc_churn | 4.3% | 1.7% | 1.5% |
| bench_empty | 3.0% | 26.9% | 4.6% |

## Optimization-level matrix (runtime median ms)

2 warmup + 3 runs per cell. Rows are the same workload compiled with a different `-O` level.

| Benchmark | O | CDD | clang | gcc | CDD/clang | CDD/gcc |
|---|---|---|---|---|---|---|
| bench_fib | O0 | 381.1 | 474.7 | 402.3 | 0.80x | 0.95x |
| bench_fib | O1 | 200.8 | 218.2 | 334.9 | 0.92x | 0.60x |
| bench_fib | O2 | 250.8 | 242.0 | 124.5 | 1.04x | 2.01x |
| bench_fib | O3 | 253.4 | 241.4 | 121.4 | 1.05x | 2.09x |
| bench_matmul | O0 | 729.5 | 843.1 | 865.9 | 0.87x | 0.84x |
| bench_matmul | O1 | 229.6 | 141.5 | 133.1 | 1.62x | 1.72x |
| bench_matmul | O2 | 205.1 | 70.2 | 71.2 | 2.92x | 2.88x |
| bench_matmul | O3 | 250.1 | 75.8 | 70.5 | 3.30x | 3.55x |
| bench_nbody | O0 | 601.4 | 1127.6 | 1352.0 | 0.53x | 0.44x |
| bench_nbody | O1 | 284.2 | 276.5 | 280.1 | 1.03x | 1.01x |
| bench_nbody | O2 | 309.9 | 302.6 | 307.9 | 1.02x | 1.01x |
| bench_nbody | O3 | 285.1 | 280.3 | 300.8 | 1.02x | 0.95x |
| bench_hashmap | O0 | 504.9 | 662.1 | 532.8 | 0.76x | 0.95x |
| bench_hashmap | O1 | 211.9 | 312.0 | 352.3 | 0.68x | 0.60x |
| bench_hashmap | O2 | 203.1 | 294.4 | 293.9 | 0.69x | 0.69x |
| bench_hashmap | O3 | 201.2 | 299.1 | 291.4 | 0.67x | 0.69x |
| bench_nqueens | O0 | 803.7 | 908.7 | 845.3 | 0.88x | 0.95x |
| bench_nqueens | O1 | 334.1 | 400.0 | 357.7 | 0.84x | 0.93x |
| bench_nqueens | O2 | 340.0 | 368.6 | 342.5 | 0.92x | 0.99x |
| bench_nqueens | O3 | 335.2 | 296.4 | 330.1 | 1.13x | 1.02x |
| bench_str_append | O0 | 145.6 | 2422.1 | 2185.7 | 0.06x | 0.07x |
| bench_str_append | O1 | 59.0 | 250.9 | 251.0 | 0.24x | 0.23x |
| bench_str_append | O2 | 61.9 | 249.7 | 250.3 | 0.25x | 0.25x |
| bench_str_append | O3 | 70.9 | 252.0 | 250.1 | 0.28x | 0.28x |

This matrix is diagnostic: it shows whether a gap is already present at `-O0` (front-end/codegen shape) or only appears after optimization.

## Compile time (median ms, lower is better)

| Source | CDD `-c` | CDD syntax | clang `-c` | gcc `-c` | clang syntax | gcc syntax |
|---|---|---|---|---|---|---|
| bench_fib | 17.0 | 11.0 | 33.4 | 43.5 | 23.7 | 10.8 |
| bench_sieve | 22.0 | 11.1 | 43.4 | 35.8 | 29.6 | 14.8 |
| bench_matmul | 20.3 | 11.1 | 45.2 | 41.5 | 30.0 | 15.1 |
| bench_nbody | 30.3 | 12.0 | 180.6 | 148.3 | 154.5 | 96.7 |
| bench_sort | 24.9 | 11.1 | 51.5 | 48.0 | 28.9 | 14.8 |
| bench_hashmap | 19.9 | 11.2 | 42.5 | 40.4 | 28.7 | 14.7 |
| bench_nqueens | 20.0 | 10.7 | 42.0 | 35.3 | 28.3 | 14.5 |
| bench_binary_search | 18.3 | 10.9 | 42.0 | 35.0 | 28.9 | 14.7 |
| bench_mandelbrot | 18.3 | 11.0 | 35.1 | 27.2 | 23.7 | 10.8 |
| bench_class | 21.3 | 11.2 | 47.0 | 39.4 | 29.5 | 14.9 |
| bench_str_append | 18.7 | 10.9 | 191.5 | 149.7 | 156.9 | 104.7 |
| bench_vec_push | 52.9 | 12.4 | 152.3 | 113.6 | 121.3 | 74.8 |
| bench_hashmap_std | 76.5 | 13.3 | 208.2 | 183.2 | 146.4 | 97.0 |
| bench_deque | 52.8 | 13.0 | 162.0 | 136.2 | 113.5 | 70.7 |
| bench_malloc_churn | 18.6 | 11.4 | 41.8 | 34.4 | 29.9 | 15.1 |
| bench_empty | 14.0 | 10.4 | 23.7 | 12.5 | 18.1 | 6.4 |
| corpus_100 | 84.1 | 13.8 | 124.6 | 192.2 | 24.4 | 9.4 |
| corpus_300 | 221.0 | 17.6 | 341.9 | 556.9 | 35.4 | 15.0 |
| corpus_600 | 431.6 | 24.9 | 686.5 | 1140.4 | 51.6 | 23.0 |
| corpus_1200 | 846.1 | 39.1 | 1464.9 | 2427.4 | 82.3 | 39.2 |

`syntax` is CDD's `-fsyntax-only` (lexer/parser/imports, no semantic analysis) versus the reference's `-fsyntax-only` (which does include semantic analysis), so the syntax columns are diagnostic, not a fair cross-language ratio.

### Compile scaling (synthetic corpus)

| Functions | CDD `-c` | clang `-c` | gcc `-c` | CDD ms/fn | clang ms/fn | gcc ms/fn |
|---|---|---|---|---|---|
| 100 | 84.1 | 124.6 | 192.2 | 0.84 | 1.25 | 1.92 |
| 300 | 221.0 | 341.9 | 556.9 | 0.74 | 1.14 | 1.86 |
| 600 | 431.6 | 686.5 | 1140.4 | 0.72 | 1.14 | 1.90 |
| 1200 | 846.1 | 1464.9 | 2427.4 | 0.71 | 1.22 | 2.02 |

`ms/fn` normalizes the fixed compiler startup cost and shows the marginal cost of one generated function.

## Code size: object sections (KiB)

| Benchmark | CDD `.text` | clang `.text` | gcc `.text` | CDD `.rodata` | CDD `.data` | CDD `.bss` | CDD debug |
|---|---|---|---|---|---|---|---|
| bench_fib | 0.1 | 0.1 | 0.9 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_sieve | 0.5 | 0.3 | 0.3 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_matmul | 0.4 | 0.6 | 0.7 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_nbody | 2.4 | 1.3 | 1.5 | 0.1 | 0.0 | 0.0 | 0.8 |
| bench_sort | 0.6 | 0.7 | 0.7 | 0.0 | 0.0 | 0.0 | 0.9 |
| bench_hashmap | 0.3 | 0.3 | 0.5 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_nqueens | 0.3 | 0.2 | 0.2 | 0.0 | 0.0 | 0.0 | 0.8 |
| bench_binary_search | 0.2 | 0.3 | 0.2 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_mandelbrot | 0.3 | 0.3 | 0.3 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_class | 0.4 | 0.4 | 0.4 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_str_append | 0.2 | 0.4 | 0.6 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_vec_push | 2.3 | 0.4 | 0.5 | 0.0 | 0.0 | 0.0 | 3.4 |
| bench_hashmap_std | 4.7 | 1.1 | 1.3 | 0.0 | 0.0 | 0.0 | 4.6 |
| bench_deque | 2.3 | 1.5 | 1.5 | 0.0 | 0.0 | 0.0 | 3.3 |
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
| bench_sort | 259.5 | 222.3 | 25.7 | 14.2 | 25.8 | 14.1 | 10.08x |
| bench_hashmap | 259.0 | 222.3 | 25.2 | 14.2 | 25.4 | 14.1 | 10.28x |
| bench_nqueens | 259.5 | 222.3 | 25.1 | 14.2 | 24.8 | 14.1 | 10.33x |
| bench_binary_search | 255.0 | 218.3 | 24.7 | 14.2 | 24.4 | 14.1 | 10.33x |
| bench_mandelbrot | 16.5 | 14.1 | 22.5 | 14.2 | 22.4 | 14.1 | 0.73x |
| bench_class | 259.2 | 222.3 | 25.6 | 14.2 | 25.2 | 14.1 | 10.13x |
| bench_str_append | 255.0 | 218.3 | 60.0 | 14.3 | 65.4 | 14.2 | 4.25x |
| bench_vec_push | 268.7 | 226.3 | 43.8 | 14.3 | 53.6 | 14.2 | 6.14x |
| bench_hashmap_std | 274.7 | 230.3 | 106.7 | 14.3 | 127.0 | 14.2 | 2.57x |
| bench_deque | 268.5 | 226.3 | 53.2 | 14.3 | 68.2 | 14.2 | 5.05x |
| bench_malloc_churn | 255.1 | 218.3 | 24.5 | 14.2 | 24.3 | 14.1 | 10.41x |
| bench_empty | 16.4 | 14.0 | 16.5 | 14.1 | 16.6 | 14.0 | 0.99x |

CDD links mimalloc statically, so its executables are much larger than the C++ references even when generated code is comparable. `stripped` removes symbols/debug information from a copy.

## LLVM IR size (diagnostic)

| Source | CDD bytes | CDD lines | clang bytes | clang lines | CDD/clang bytes |
|---|---|---|---|---|---|
| bench_fib | 3120 | 117 | 38693 | 427 | 0.08x |
| bench_sieve | 8053 | 225 | 63323 | 738 | 0.13x |
| bench_matmul | 11846 | 296 | 67026 | 789 | 0.18x |
| bench_nbody | 42299 | 873 | 137391 | 1553 | 0.31x |
| bench_sort | 17801 | 423 | 79927 | 1021 | 0.22x |
| bench_hashmap | 15630 | 373 | 69092 | 836 | 0.23x |
| bench_nqueens | 9811 | 265 | 64928 | 748 | 0.15x |
| bench_binary_search | 8035 | 226 | 63087 | 737 | 0.13x |
| bench_mandelbrot | 7921 | 218 | 46184 | 560 | 0.17x |
| bench_class | 12685 | 330 | 74112 | 877 | 0.17x |
| bench_str_append | 7593 | 201 | 222822 | 2166 | 0.03x |
| bench_vec_push | 80402 | 1828 | 161109 | 1704 | 0.50x |
| bench_hashmap_std | 111204 | 2463 | 409521 | 3952 | 0.27x |
| bench_deque | 74592 | 1660 | 250127 | 2910 | 0.30x |
| bench_malloc_churn | 7292 | 201 | 61282 | 705 | 0.12x |
| bench_empty | 1765 | 85 | 2317 | 36 | 0.76x |
| corpus_100 | 187869 | 4290 | 378316 | 5043 | 0.50x |
| corpus_300 | 564595 | 12690 | 1147139 | 15043 | 0.49x |
| corpus_600 | 1137385 | 25290 | 2300952 | 30043 | 0.49x |
| corpus_1200 | 2291386 | 50490 | 4650481 | 60043 | 0.49x |

Both are LLVM IR, but front ends attach different metadata and debug information; use this as a diagnostic for CDD's own IR growth, not as a strict cross-language size ratio.

## Peak memory: runtime (MiB)

| Benchmark | CDD | clang | gcc | CDD/clang |
|---|---|---|---|---|
| bench_fib | 1.7 | 4.1 | 1.7 | 0.41x |
| bench_sieve | 196.0 | 194.6 | 192.4 | 1.01x |
| bench_matmul | 18.0 | 17.3 | 15.2 | 1.04x |
| bench_nbody | 4.4 | 4.1 | 1.7 | 1.09x |
| bench_sort | 34.1 | 34.3 | 32.2 | 0.99x |
| bench_hashmap | 166.1 | 162.3 | 160.3 | 1.02x |
| bench_nqueens | 4.0 | 4.1 | 1.7 | 0.99x |
| bench_binary_search | 70.1 | 67.8 | 65.7 | 1.03x |
| bench_mandelbrot | 1.7 | 4.1 | 1.7 | 0.41x |
| bench_class | 50.0 | 49.6 | 47.5 | 1.01x |
| bench_str_append | 185.8 | 243.7 | 243.6 | 0.76x |
| bench_vec_push | 181.8 | 131.7 | 131.6 | 1.38x |
| bench_hashmap_std | 166.1 | 186.7 | 186.7 | 0.89x |
| bench_deque | 181.7 | 131.7 | 131.7 | 1.38x |
| bench_malloc_churn | 418.1 | 507.4 | 505.3 | 0.82x |
| bench_empty | 1.4 | 3.8 | 1.4 | 0.37x |

Peak RSS is measured once per implementation and includes the static mimalloc arena for CDD; allocator caching can make it sensitive to run order.

### Peak memory: compile (MiB)

| Source | CDD | clang | gcc |
|---|---|---|---|
| bench_fib | 81.3 | 104.7 | 31.4 |
| bench_sieve | 87.1 | 106.6 | 32.5 |
| bench_matmul | 86.3 | 106.9 | 34.7 |
| bench_nbody | 88.1 | 120.5 | 64.3 |
| bench_sort | 82.0 | 107.3 | 33.9 |
| bench_hashmap | 81.7 | 105.7 | 32.6 |
| bench_nqueens | 81.6 | 106.4 | 31.8 |
| bench_binary_search | 82.0 | 106.7 | 32.2 |
| bench_mandelbrot | 85.8 | 105.2 | 30.5 |
| bench_class | 82.2 | 107.5 | 33.8 |
| bench_str_append | 82.0 | 121.7 | 68.0 |
| bench_vec_push | 88.9 | 118.9 | 56.3 |
| bench_hashmap_std | 92.6 | 122.8 | 68.4 |
| bench_deque | 92.7 | 118.1 | 57.7 |
| bench_malloc_churn | 81.5 | 105.6 | 31.7 |
| bench_empty | 74.7 | 96.3 | 23.4 |
| corpus_100 | 86.6 | 103.9 | 35.3 |
| corpus_300 | 98.7 | 111.6 | 52.5 |
| corpus_600 | 122.6 | 122.9 | 75.1 |
| corpus_1200 | 152.6 | 146.4 | 121.6 |

## Hardware counters (one representative run)

| Benchmark | Impl | cycles (M) | instructions (M) | IPC | GHz | cache-miss % | branch-miss % |
|---|---|---|---|---|---|---|---|
| bench_fib | cdd | 1112.6 | 3646.8 | 3.28 | 4.65 | 0.00 | 0.20 |
| bench_fib | clang | 1046.8 | 3648.9 | 3.49 | 4.65 | 0.00 | 0.20 |
| bench_fib | gcc | 574.0 | 2762.1 | 4.81 | 4.65 | 0.00 | 0.41 |
| bench_sieve | cdd | 5341.9 | 3821.6 | 0.72 | 4.65 | 3.60 | 0.14 |
| bench_sieve | clang | 5592.8 | 2512.9 | 0.45 | 4.65 | 5.87 | 1.16 |
| bench_sieve | gcc | 5665.8 | 2484.8 | 0.44 | 4.65 | 5.96 | 1.16 |
| bench_matmul | cdd | 935.8 | 3649.0 | 3.90 | 4.64 | 0.04 | 0.15 |
| bench_matmul | clang | 318.6 | 1342.7 | 4.21 | 4.64 | 0.14 | 0.82 |
| bench_matmul | gcc | 387.4 | 1849.1 | 4.77 | 4.63 | 0.10 | 0.47 |
| bench_nbody | cdd | 1337.5 | 3475.0 | 2.60 | 4.65 | 0.00 | 0.07 |
| bench_nbody | clang | 1371.1 | 3011.6 | 2.20 | 4.65 | 0.00 | 0.12 |
| bench_nbody | gcc | 1346.9 | 2949.5 | 2.19 | 4.65 | 0.00 | 0.15 |
| bench_sort | cdd | 2146.4 | 2116.1 | 0.99 | 4.65 | 0.05 | 17.86 |
| bench_sort | clang | 2067.2 | 1956.3 | 0.95 | 4.65 | 0.06 | 20.13 |
| bench_sort | gcc | 2101.4 | 2186.0 | 1.04 | 4.65 | 0.05 | 20.07 |
| bench_hashmap | cdd | 937.6 | 365.7 | 0.39 | 4.65 | 8.02 | 0.20 |
| bench_hashmap | clang | 1765.0 | 579.3 | 0.33 | 4.65 | 6.86 | 4.24 |
| bench_hashmap | gcc | 1325.0 | 540.7 | 0.41 | 4.65 | 7.21 | 4.84 |
| bench_nqueens | cdd | 1551.6 | 4273.5 | 2.75 | 4.65 | 0.00 | 2.93 |
| bench_nqueens | clang | 1966.6 | 4024.5 | 2.05 | 4.65 | 0.00 | 2.94 |
| bench_nqueens | gcc | 1589.7 | 4124.0 | 2.59 | 4.65 | 0.00 | 2.91 |
| bench_binary_search | cdd | 3134.0 | 632.9 | 0.20 | 4.65 | 33.30 | 15.66 |
| bench_binary_search | clang | 3184.0 | 654.2 | 0.21 | 4.65 | 18.51 | 16.61 |
| bench_binary_search | gcc | 3214.6 | 570.0 | 0.18 | 4.65 | 20.55 | 16.43 |
| bench_mandelbrot | cdd | 1754.0 | 2964.9 | 1.69 | 4.65 | 0.00 | 0.11 |
| bench_mandelbrot | clang | 1757.0 | 2706.6 | 1.54 | 4.65 | 0.00 | 0.16 |
| bench_mandelbrot | gcc | 1752.9 | 2784.2 | 1.59 | 4.65 | 0.00 | 0.11 |
| bench_class | cdd | 1215.2 | 5234.0 | 4.31 | 4.64 | 0.02 | 0.05 |
| bench_class | clang | 1049.8 | 3246.1 | 3.09 | 4.65 | 0.05 | 0.58 |
| bench_class | gcc | 1076.7 | 4246.2 | 3.94 | 4.65 | 0.04 | 0.57 |
| bench_str_append | cdd | 342.0 | 805.1 | 2.35 | 4.64 | 0.10 | 0.13 |
| bench_str_append | clang | 1135.4 | 1301.7 | 1.15 | 4.65 | 0.36 | 5.01 |
| bench_str_append | gcc | 1161.7 | 1404.8 | 1.21 | 4.65 | 0.33 | 5.19 |
| bench_vec_push | cdd | 239.4 | 633.8 | 2.65 | 4.64 | 0.17 | 0.16 |
| bench_vec_push | clang | 628.6 | 707.8 | 1.13 | 4.65 | 0.43 | 5.56 |
| bench_vec_push | gcc | 642.7 | 763.9 | 1.19 | 4.65 | 0.42 | 4.78 |
| bench_hashmap_std | cdd | 1537.1 | 1359.3 | 0.88 | 4.65 | 1.72 | 4.17 |
| bench_hashmap_std | clang | 7365.7 | 2441.2 | 0.33 | 4.65 | 2.56 | 3.18 |
| bench_hashmap_std | gcc | 6609.5 | 2341.6 | 0.35 | 4.65 | 2.65 | 3.33 |
| bench_deque | cdd | 585.8 | 1182.9 | 2.02 | 4.64 | 0.05 | 0.08 |
| bench_deque | clang | 493.3 | 1040.2 | 2.11 | 4.65 | 0.16 | 2.26 |
| bench_deque | gcc | 432.9 | 844.0 | 1.95 | 4.65 | 0.20 | 2.63 |
| bench_malloc_churn | cdd | 699.4 | 552.9 | 0.79 | 4.65 | 4.63 | 0.44 |
| bench_malloc_churn | clang | 2221.5 | 2827.1 | 1.27 | 4.65 | 1.65 | 2.41 |
| bench_malloc_churn | gcc | 2186.5 | 2847.5 | 1.30 | 4.65 | 1.63 | 2.40 |
| bench_empty | cdd | 1.1 | 0.8 | 0.72 | 4.45 | 2.98 | 12.34 |
| bench_empty | clang | 2.8 | 3.2 | 1.12 | 4.60 | 1.39 | 7.51 |
| bench_empty | gcc | 1.1 | 0.8 | 0.73 | 4.61 | 2.93 | 12.35 |

## Analysis and optimization targets

- Largest core gaps vs clang: `bench_matmul` 2.91x, `bench_class` 1.13x, `bench_fib` 1.06x.
- Largest core wins vs clang: `bench_sieve` 0.89x, `bench_nqueens` 0.79x, `bench_hashmap` 0.70x.
- `bench_matmul` is the clearest code-generation gap. The loop order is already cache-friendly; the likely missing pieces are alias information and loop vectorization. Check the matrix's O1/O2/O3 cells and the generated IR for vector widths before changing codegen.
- `bench_fib` is close to clang but well behind gcc. GCC emits a different recursive shape/inlining decision; treat this as a codegen-shape question, not a front-end question.
- `bench_nqueens`, `bench_binary_search`, `bench_mandelbrot`, and `bench_class` extend the core coverage to backtracking, branchy memory access, floating-point control flow, and by-value method calls. They make it harder to improve the core geomean by tuning one hot loop.
- `stdlib` gaps reflect both generated code and the CDD implementation (`string`, `Vec`, `HashMap`, `Deque`). Keep them separate from core when judging the compiler backend.
- `alloc` is dominated by mimalloc versus the platform allocator; `startup` is process overhead. Neither should drive backend optimization decisions.
- Highest-value follow-ups for `P3-01`: matmul vectorization/alias metadata, fib code shape, and stdlib growth paths (`string`, `Vec`). Use `compare.py` to quantify each change against this baseline.

## Caveats

- The CDD compiler uses LLVM 21.1.8; `clang++` may use a different LLVM release. The `g++` column is an independent GCC reference.
- The main tables use `-O2`; the optimization-level matrix covers `-O0..-O3`. No `-march=native`, no fast-math. The target is the x86-64 baseline, not a tuned build.
- The CPU governor is read live; unless it is `performance`, boost behaviour adds run-to-run noise. Medians over the measured runs and CPU pinning keep this small but not zero; the CV table makes the spread explicit.
- This baseline may be taken on a shared desktop (`load_average` start and end are recorded above). The runner auto-selects the least busy core and interleaves implementations to keep the comparison fair, but an idle machine will produce lower absolute times. Ratios are the intended comparison metric.
- Runtime includes process start and exit (typically 1-3 ms); the `startup` tier measures that overhead directly.
- The `alloc` tier compares CDD's static mimalloc against the C++ reference's system allocator; read it as a product-level number, not an allocator-to-allocator contest. It is excluded from the core geomean.
- `stdlib` results depend on both code generation and the standard library implementation. `std::HashMap`/`std::unordered_map` and `std::Deque`/`std::deque` have different growth and bucket strategies; these rows are product-level comparisons.
- Compile time on the small benchmark sources is dominated by process startup and the automatic std prelude; use the `corpus_*` rows and their `ms/fn` columns for throughput comparisons.
- LLVM IR size is a diagnostic for CDD's own optimizer growth. The two front ends attach different metadata/debug information, so the IR ratio is not a strict cross-language code-size metric.
- Peak RSS is sampled once per implementation; allocator caching and the static mimalloc arena make it sensitive to run order.
- `startup` hardware counters are dominated by process setup and are only informative for the wall-clock column.

