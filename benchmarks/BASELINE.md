# CDD Performance Baseline — 2026-11-17-baseline

Generated: `2026-11-17T22:43:23`  
Command: `/home/ares/apps/conda/bin/python3 benchmarks/run.py --full --baseline-md --label 2026-11-17-baseline`

## Environment

| Item | Value |
|---|---|
| CPU | AMD Ryzen 5 5600X 6-Core Processor |
| CPU count / pinned | 12 / 0 |
| Governor | powersave |
| Load average (1/5/15 min) | 0.82 / 1.23 / 1.33 |
| Load average at end | 1.87 / 1.74 / 1.51 |
| Selected core busy at start | 0.0% |
| Kernel | 7.0.0-43-generic (x86_64) |
| CDD | `/media/nvm1/workspace/Cdotdot/build/bin/cdd` (3278352 bytes, build type `Release`) |
| CDD commit | `388e62c` master (dirty worktree) |
| LLVM (llvm-config) | 21.1.8 |
| clang | Ubuntu clang version 22.1.2 (1ubuntu1) |
| gcc | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |

Method: `-O2` for every compiler, 5 warmup + 9 measured runs (median) for runtime, 1 warmup + 5 runs for compile time, one implementation round at a time (interleaved), process pinned with `taskset`. Each row runs the same algorithm, data types and workload size in CDD and C++. C++ references use `-g` because CDD emits debug information by default. The report also records peak RSS, LLVM IR size, object-section breakdown, stripped executable size, and an optional `-O0..-O3` matrix.

## Headline

- `core` geometric mean: **1.06x** vs clang, **1.15x** vs gcc
- `stdlib` geometric mean: **0.41x** vs clang, **0.43x** vs gcc
- `alloc` geometric mean: **0.30x** vs clang, **0.30x** vs gcc
- `startup` geometric mean: **0.69x** vs clang, **1.01x** vs gcc
- Largest core gap vs clang: `bench_matmul` at **2.94x**; largest core win: `bench_hashmap` at **0.66x**.

## Runtime (median, lower is better; ratio = CDD / reference)

| Benchmark | Tier | CDD (ms) | clang (ms) | gcc (ms) | CDD/clang | CDD/gcc | checksum |
|---|---|---|---|---|---|---|---|
| bench_fib | core | 240.8 | 228.1 | 125.3 | 1.06x | 1.92x | 102334155 |
| bench_sieve | core | 1072.6 | 1192.3 | 1190.1 | 0.90x | 0.90x | 11078937 |
| bench_matmul | core | 207.8 | 70.7 | 73.4 | 2.94x | 2.83x | 2876 |
| bench_nbody | core | 285.9 | 298.0 | 295.3 | 0.96x | 0.97x | 1690136770432667 |
| bench_sort | core | 470.0 | 451.1 | 461.1 | 1.04x | 1.02x | 8588201207136256 |
| bench_hashmap | core | 206.3 | 314.9 | 294.9 | 0.66x | 0.70x | 4000000 |
| bench_nqueens | core | 342.1 | 400.6 | 345.4 | 0.85x | 0.99x | 73712 |
| bench_binary_search | core | 697.0 | 687.6 | 665.1 | 1.01x | 1.05x | 8382020676000 |
| bench_mandelbrot | core | 382.4 | 382.6 | 380.8 | 1.00x | 1.00x | 172812923 |
| bench_class | core | 263.4 | 222.9 | 227.4 | 1.18x | 1.16x | 67884192 |
| bench_str_append | stdlib | 60.8 | 237.8 | 239.8 | 0.26x | 0.25x | 8976000000 |
| bench_vec_push | stdlib | 53.6 | 135.6 | 136.7 | 0.40x | 0.39x | 1535999952000000 |
| bench_hashmap_std | stdlib | 348.2 | 1532.8 | 1401.1 | 0.23x | 0.25x | 3999998000000 |
| bench_deque | stdlib | 128.2 | 107.6 | 94.5 | 1.19x | 1.36x | 511999984000000 |
| bench_malloc_churn | alloc | 149.3 | 498.6 | 497.0 | 0.30x | 0.30x | 1524000000 |
| bench_empty | startup | 1.0 | 1.5 | 1.0 | 0.69x | 1.01x |  |

### Geometric mean by tier

| Tier | CDD/clang | CDD/gcc |
|---|---|---|
| core (10) | 1.06x | 1.15x |
| stdlib (4) | 0.41x | 0.43x |
| alloc (1) | 0.30x | 0.30x |
| startup (1) | 0.69x | 1.01x |

### Run-to-run stability (CV %, lower is better)

| Benchmark | CDD CV | clang CV | gcc CV |
|---|---|---|---|
| bench_fib | 1.4% | 6.9% | 12.4% |
| bench_sieve | 2.6% | 1.9% | 2.5% |
| bench_matmul | 1.2% | 3.1% | 2.5% |
| bench_nbody | 0.3% | 0.6% | 0.6% |
| bench_sort | 1.0% | 2.9% | 2.9% |
| bench_hashmap | 3.9% | 5.2% | 3.7% |
| bench_nqueens | 1.0% | 4.4% | 3.6% |
| bench_binary_search | 3.2% | 2.9% | 4.1% |
| bench_mandelbrot | 2.1% | 1.3% | 0.2% |
| bench_class | 2.7% | 2.8% | 3.1% |
| bench_str_append | 5.4% | 0.6% | 1.4% |
| bench_vec_push | 7.3% | 1.3% | 1.4% |
| bench_hashmap_std | 3.5% | 3.1% | 3.2% |
| bench_deque | 0.4% | 0.7% | 0.6% |
| bench_malloc_churn | 3.1% | 1.4% | 3.0% |
| bench_empty | 4.4% | 3.2% | 3.1% |

## Optimization-level matrix (runtime median ms)

2 warmup + 3 runs per cell. Rows are the same workload compiled with a different `-O` level.

| Benchmark | O | CDD | clang | gcc | CDD/clang | CDD/gcc |
|---|---|---|---|---|---|---|
| bench_fib | O0 | 376.5 | 464.3 | 414.9 | 0.81x | 0.91x |
| bench_fib | O1 | 186.4 | 223.0 | 339.3 | 0.84x | 0.55x |
| bench_fib | O2 | 240.6 | 229.5 | 126.1 | 1.05x | 1.91x |
| bench_fib | O3 | 241.4 | 227.8 | 121.6 | 1.06x | 1.98x |
| bench_matmul | O0 | - | - | - | - | - |
| bench_matmul | O1 | 208.5 | 121.3 | 128.8 | 1.72x | 1.62x |
| bench_matmul | O2 | 208.4 | 71.0 | 72.0 | 2.94x | 2.89x |
| bench_matmul | O3 | 206.5 | 70.1 | 64.7 | 2.95x | 3.19x |
| bench_nbody | O0 | - | - | - | - | - |
| bench_nbody | O1 | 324.5 | 276.3 | 280.3 | 1.17x | 1.16x |
| bench_nbody | O2 | 285.1 | 293.7 | 292.7 | 0.97x | 0.97x |
| bench_nbody | O3 | 281.9 | 282.3 | 302.8 | 1.00x | 0.93x |
| bench_hashmap | O0 | - | - | - | - | - |
| bench_hashmap | O1 | 199.6 | 302.9 | 326.8 | 0.66x | 0.61x |
| bench_hashmap | O2 | 195.3 | 288.5 | 282.6 | 0.68x | 0.69x |
| bench_hashmap | O3 | 211.4 | 291.3 | 277.1 | 0.73x | 0.76x |
| bench_nqueens | O0 | 902.2 | 918.2 | 856.1 | 0.98x | 1.05x |
| bench_nqueens | O1 | 340.5 | 396.0 | 360.7 | 0.86x | 0.94x |
| bench_nqueens | O2 | 336.6 | 399.8 | 345.6 | 0.84x | 0.97x |
| bench_nqueens | O3 | 337.2 | 300.6 | 331.8 | 1.12x | 1.02x |
| bench_str_append | O0 | 156.9 | 2372.9 | 2170.4 | 0.07x | 0.07x |
| bench_str_append | O1 | 58.0 | 252.4 | 252.4 | 0.23x | 0.23x |
| bench_str_append | O2 | 57.0 | 240.0 | 243.7 | 0.24x | 0.23x |
| bench_str_append | O3 | 57.4 | 242.8 | 239.7 | 0.24x | 0.24x |

Cells with build/run errors: `bench_matmul O0 cdd`, `bench_nbody O0 cdd`, `bench_hashmap O0 cdd`.

This matrix is diagnostic: it shows whether a gap is already present at `-O0` (front-end/codegen shape) or only appears after optimization.

## Compile time (median ms, lower is better)

| Source | CDD `-c` | CDD syntax | clang `-c` | gcc `-c` | clang syntax | gcc syntax |
|---|---|---|---|---|---|---|
| bench_fib | 15.7 | 9.9 | 32.2 | 42.5 | 22.6 | 10.6 |
| bench_sieve | 20.8 | 10.6 | 42.7 | 34.6 | 28.4 | 14.5 |
| bench_matmul | 19.2 | 10.5 | 44.4 | 39.8 | 28.3 | 14.7 |
| bench_nbody | 28.6 | 11.2 | 173.4 | 141.2 | 148.0 | 92.1 |
| bench_sort | 23.8 | 10.5 | 51.5 | 47.5 | 28.1 | 14.6 |
| bench_hashmap | 19.0 | 10.5 | 42.1 | 40.1 | 28.0 | 14.5 |
| bench_nqueens | 19.1 | 10.3 | 41.7 | 35.0 | 28.6 | 14.4 |
| bench_binary_search | 17.4 | 10.3 | 42.6 | 34.2 | 28.0 | 14.4 |
| bench_mandelbrot | 17.2 | 10.1 | 34.3 | 26.8 | 22.8 | 10.8 |
| bench_class | 20.5 | 10.5 | 46.4 | 39.1 | 28.8 | 14.7 |
| bench_str_append | 17.9 | 10.1 | 186.5 | 150.7 | 156.7 | 104.9 |
| bench_vec_push | 52.0 | 11.9 | 151.5 | 113.5 | 123.2 | 74.4 |
| bench_hashmap_std | 76.0 | 12.8 | 209.2 | 177.7 | 147.7 | 95.9 |
| bench_deque | 51.9 | 12.4 | 162.0 | 135.2 | 112.2 | 70.6 |
| bench_malloc_churn | 17.1 | 10.2 | 39.7 | 33.0 | 27.8 | 14.5 |
| bench_empty | 13.2 | 9.6 | 22.0 | 12.3 | 16.6 | 6.1 |
| corpus_100 | 82.2 | 13.1 | 122.9 | 190.3 | 23.3 | 9.3 |
| corpus_300 | 217.2 | 17.1 | 332.8 | 553.4 | 33.7 | 14.6 |
| corpus_600 | 423.9 | 23.7 | 667.9 | 1127.4 | 49.8 | 22.6 |
| corpus_1200 | 841.9 | 37.3 | 1423.6 | 2374.6 | 80.9 | 38.2 |

`syntax` is CDD's `-fsyntax-only` (lexer/parser/imports, no semantic analysis) versus the reference's `-fsyntax-only` (which does include semantic analysis), so the syntax columns are diagnostic, not a fair cross-language ratio.

### Compile scaling (synthetic corpus)

| Functions | CDD `-c` | clang `-c` | gcc `-c` | CDD ms/fn | clang ms/fn | gcc ms/fn |
|---|---|---|---|---|---|
| 100 | 82.2 | 122.9 | 190.3 | 0.82 | 1.23 | 1.90 |
| 300 | 217.2 | 332.8 | 553.4 | 0.72 | 1.11 | 1.84 |
| 600 | 423.9 | 667.9 | 1127.4 | 0.71 | 1.11 | 1.88 |
| 1200 | 841.9 | 1423.6 | 2374.6 | 0.70 | 1.19 | 1.98 |

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
| bench_mandelbrot | 0.3 | 0.3 | 0.3 | 0.1 | 0.0 | 0.0 | 0.5 |
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
| bench_malloc_churn | 7296 | 201 | 61282 | 705 | 0.12x |
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
| bench_sieve | 196.1 | 194.5 | 192.4 | 1.01x |
| bench_matmul | 18.0 | 17.3 | 15.2 | 1.04x |
| bench_nbody | 4.5 | 4.1 | 1.7 | 1.10x |
| bench_sort | 34.0 | 34.3 | 32.2 | 0.99x |
| bench_hashmap | 166.0 | 162.3 | 160.3 | 1.02x |
| bench_nqueens | 4.0 | 4.1 | 1.7 | 0.98x |
| bench_binary_search | 70.1 | 67.7 | 65.7 | 1.04x |
| bench_mandelbrot | 1.7 | 4.1 | 1.7 | 0.41x |
| bench_class | 50.0 | 49.5 | 47.5 | 1.01x |
| bench_str_append | 185.8 | 243.7 | 243.7 | 0.76x |
| bench_vec_push | 181.8 | 131.6 | 131.6 | 1.38x |
| bench_hashmap_std | 166.1 | 186.6 | 186.7 | 0.89x |
| bench_deque | 181.7 | 131.7 | 131.6 | 1.38x |
| bench_malloc_churn | 418.1 | 507.4 | 505.3 | 0.82x |
| bench_empty | 1.4 | 3.8 | 1.4 | 0.37x |

Peak RSS is measured once per implementation and includes the static mimalloc arena for CDD; allocator caching can make it sensitive to run order.

### Peak memory: compile (MiB)

| Source | CDD | clang | gcc |
|---|---|---|---|
| bench_fib | 81.3 | 104.7 | 31.4 |
| bench_sieve | 87.0 | 106.6 | 32.6 |
| bench_matmul | 86.1 | 106.9 | 34.7 |
| bench_nbody | 87.9 | 120.5 | 64.4 |
| bench_sort | 81.8 | 107.2 | 33.9 |
| bench_hashmap | 81.6 | 105.8 | 32.7 |
| bench_nqueens | 81.5 | 106.5 | 31.8 |
| bench_binary_search | 81.9 | 106.6 | 32.2 |
| bench_mandelbrot | 85.6 | 105.2 | 30.6 |
| bench_class | 82.0 | 107.5 | 33.6 |
| bench_str_append | 81.9 | 121.7 | 67.8 |
| bench_vec_push | 88.9 | 118.8 | 56.2 |
| bench_hashmap_std | 92.3 | 122.7 | 68.5 |
| bench_deque | 92.6 | 118.2 | 57.6 |
| bench_malloc_churn | 81.5 | 105.7 | 31.8 |
| bench_empty | 74.5 | 96.3 | 23.4 |
| corpus_100 | 86.4 | 104.0 | 35.4 |
| corpus_300 | 98.4 | 111.3 | 52.3 |
| corpus_600 | 122.5 | 122.9 | 75.2 |
| corpus_1200 | 148.6 | 146.4 | 121.5 |

## Hardware counters (one representative run)

| Benchmark | Impl | cycles (M) | instructions (M) | IPC | GHz | cache-miss % | branch-miss % |
|---|---|---|---|---|---|---|---|
| bench_fib | cdd | 1115.6 | 3647.7 | 3.27 | 4.64 | 0.00 | 0.20 |
| bench_fib | clang | 1051.8 | 3649.3 | 3.47 | 4.65 | 0.00 | 0.20 |
| bench_fib | gcc | 575.8 | 2762.3 | 4.80 | 4.64 | 0.00 | 0.41 |
| bench_sieve | cdd | 4929.4 | 3824.5 | 0.78 | 4.64 | 3.52 | 0.17 |
| bench_sieve | clang | 5515.5 | 2520.9 | 0.46 | 4.64 | 5.81 | 1.19 |
| bench_sieve | gcc | 5509.8 | 2490.0 | 0.45 | 4.65 | 5.91 | 1.18 |
| bench_matmul | cdd | 951.5 | 3650.3 | 3.84 | 4.59 | 0.05 | 0.16 |
| bench_matmul | clang | 337.2 | 1343.1 | 3.98 | 4.60 | 0.14 | 0.84 |
| bench_matmul | gcc | 328.9 | 1849.3 | 5.62 | 4.59 | 0.10 | 0.47 |
| bench_nbody | cdd | 1348.9 | 3477.1 | 2.58 | 4.64 | 0.00 | 0.09 |
| bench_nbody | clang | 1388.2 | 3013.2 | 2.17 | 4.64 | 0.01 | 0.15 |
| bench_nbody | gcc | 1379.5 | 2952.2 | 2.14 | 4.63 | 0.01 | 0.24 |
| bench_sort | cdd | 2148.9 | 2116.7 | 0.99 | 4.64 | 0.05 | 17.85 |
| bench_sort | clang | 2094.1 | 1958.3 | 0.94 | 4.64 | 0.07 | 20.13 |
| bench_sort | gcc | 2135.3 | 2188.7 | 1.03 | 4.64 | 0.06 | 20.05 |
| bench_hashmap | cdd | 904.2 | 365.4 | 0.40 | 4.64 | 8.04 | 0.21 |
| bench_hashmap | clang | 1289.1 | 580.4 | 0.45 | 4.65 | 6.73 | 4.26 |
| bench_hashmap | gcc | 1316.7 | 540.5 | 0.41 | 4.64 | 7.30 | 4.88 |
| bench_nqueens | cdd | 1575.1 | 4274.9 | 2.71 | 4.64 | 0.00 | 2.94 |
| bench_nqueens | clang | 1792.5 | 4025.6 | 2.25 | 4.64 | 0.00 | 2.96 |
| bench_nqueens | gcc | 1594.8 | 4125.5 | 2.59 | 4.64 | 0.00 | 2.91 |
| bench_binary_search | cdd | 3293.7 | 637.9 | 0.19 | 4.65 | 33.14 | 15.64 |
| bench_binary_search | clang | 3071.8 | 659.3 | 0.21 | 4.64 | 18.64 | 16.59 |
| bench_binary_search | gcc | 3166.7 | 574.4 | 0.18 | 4.64 | 20.38 | 16.41 |
| bench_mandelbrot | cdd | 1768.4 | 2793.8 | 1.58 | 4.64 | 0.00 | 0.12 |
| bench_mandelbrot | clang | 1767.6 | 2707.7 | 1.53 | 4.64 | 0.00 | 0.16 |
| bench_mandelbrot | gcc | 1764.8 | 2785.4 | 1.58 | 4.64 | 0.00 | 0.12 |
| bench_class | cdd | 1180.5 | 5234.9 | 4.43 | 4.60 | 0.02 | 0.05 |
| bench_class | clang | 1030.8 | 3247.2 | 3.15 | 4.63 | 0.05 | 0.59 |
| bench_class | gcc | 1017.7 | 4247.2 | 4.17 | 4.61 | 0.04 | 0.58 |
| bench_str_append | cdd | 276.6 | 804.3 | 2.91 | 4.63 | 0.10 | 0.13 |
| bench_str_append | clang | 1116.5 | 1303.1 | 1.17 | 4.65 | 0.41 | 5.05 |
| bench_str_append | gcc | 1129.3 | 1408.1 | 1.25 | 4.64 | 0.37 | 5.23 |
| bench_vec_push | cdd | 210.7 | 634.0 | 3.01 | 4.63 | 0.11 | 0.16 |
| bench_vec_push | clang | 618.1 | 707.8 | 1.15 | 4.64 | 0.46 | 5.57 |
| bench_vec_push | gcc | 639.3 | 763.9 | 1.19 | 4.63 | 0.42 | 4.79 |
| bench_hashmap_std | cdd | 1669.5 | 1375.2 | 0.82 | 4.64 | 1.71 | 4.08 |
| bench_hashmap_std | clang | 7665.0 | 2464.2 | 0.32 | 4.65 | 2.64 | 3.30 |
| bench_hashmap_std | gcc | 6987.2 | 2361.0 | 0.34 | 4.64 | 2.70 | 3.42 |
| bench_deque | cdd | 588.9 | 1183.3 | 2.01 | 4.63 | 0.05 | 0.08 |
| bench_deque | clang | 493.9 | 1040.3 | 2.11 | 4.64 | 0.16 | 2.27 |
| bench_deque | gcc | 430.1 | 844.4 | 1.96 | 4.63 | 0.20 | 2.64 |
| bench_malloc_churn | cdd | 684.4 | 554.1 | 0.81 | 4.64 | 4.60 | 0.46 |
| bench_malloc_churn | clang | 2183.6 | 2828.4 | 1.30 | 4.65 | 1.65 | 2.41 |
| bench_malloc_churn | gcc | 2181.4 | 2849.2 | 1.31 | 4.64 | 1.60 | 2.41 |
| bench_empty | cdd | 1.0 | 0.8 | 0.75 | 3.15 | 3.00 | 12.34 |
| bench_empty | clang | 5.6 | 3.4 | 0.61 | 3.90 | 1.88 | 8.18 |
| bench_empty | gcc | 1.1 | 0.8 | 0.72 | 3.26 | 3.30 | 12.39 |

## Analysis and optimization targets

- Largest core gaps vs clang: `bench_matmul` 2.94x, `bench_class` 1.18x, `bench_fib` 1.06x.
- Largest core wins vs clang: `bench_sieve` 0.90x, `bench_nqueens` 0.85x, `bench_hashmap` 0.66x.
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
- `cdd -O0` currently crashes on `bench_matmul`, `bench_nbody`, and `bench_hashmap` (SIGSEGV); those matrix cells are reported as errors. This is tracked under `CG-15` and is independent of the `-O2` results.
- `startup` hardware counters are dominated by process setup and are only informative for the wall-clock column.

