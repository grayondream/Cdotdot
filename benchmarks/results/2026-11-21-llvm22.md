# CDD Performance Baseline — 2026-11-21-llvm22

Generated: `2026-11-21T22:27:24`
Command: `/home/ares/apps/conda/bin/python3 benchmarks/run.py --full --baseline-md --label 2026-11-21-llvm22`

## Environment

| Item | Value |
|---|---|
| CPU | AMD Ryzen 5 5600X 6-Core Processor |
| CPU count / pinned | 12 / 0 |
| Governor | powersave |
| Load average (1/5/15 min) | 7.87 / 5.06 / 3.89 |
| Load average at end | 1.31 / 1.87 / 2.69 |
| Selected core busy at start | 0.0% |
| Kernel | 7.0.0-43-generic (x86_64) |
| CDD | `/media/nvm1/workspace/Cdotdot/build/bin/cdd` (3305272 bytes, build type `Release`) |
| CDD commit | `0d8a7ae` master |
| LLVM (llvm-config) | 21.1.8 |
| clang | Ubuntu clang version 22.1.2 (1ubuntu1) |
| gcc | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |

Method: `-O2` for every compiler, 5 warmup + 9 measured runs (median) for runtime, 1 warmup + 5 runs for compile time, one implementation round at a time (interleaved), process pinned with `taskset`. Each row runs the same algorithm, data types and workload size in CDD and C++. C++ references use `-g` because CDD emits debug information by default. The report also records peak RSS, LLVM IR size, object-section breakdown, stripped executable size, and an optional `-O0..-O3` matrix.

## Headline

- `core` geometric mean: **0.95x** vs clang, **1.03x** vs gcc
- `stdlib` geometric mean: **0.35x** vs clang, **0.37x** vs gcc
- `alloc` geometric mean: **0.29x** vs clang, **0.30x** vs gcc
- `startup` geometric mean: **0.68x** vs clang, **0.99x** vs gcc
- Largest core gap vs clang: `bench_fib` at **1.06x**; largest core win: `bench_hashmap` at **0.68x**.

## Runtime (median, lower is better; ratio = CDD / reference)

| Benchmark | Tier | CDD (ms) | clang (ms) | gcc (ms) | CDD/clang | CDD/gcc | checksum |
|---|---|---|---|---|---|---|---|
| bench_fib | core | 241.9 | 229.1 | 126.0 | 1.06x | 1.92x | 102334155 |
| bench_sieve | core | 1046.6 | 1204.5 | 1201.1 | 0.87x | 0.87x | 11078937 |
| bench_matmul | core | 62.4 | 71.3 | 73.3 | 0.88x | 0.85x | 2876 |
| bench_nbody | core | 281.3 | 295.9 | 293.3 | 0.95x | 0.96x | 1690136770432667 |
| bench_sort | core | 462.3 | 447.3 | 453.1 | 1.03x | 1.02x | 8588201207136256 |
| bench_hashmap | core | 203.0 | 297.9 | 285.6 | 0.68x | 0.71x | 4000000 |
| bench_nqueens | core | 415.5 | 418.9 | 344.9 | 0.99x | 1.20x | 73712 |
| bench_binary_search | core | 692.6 | 658.8 | 657.9 | 1.05x | 1.05x | 8382020676000 |
| bench_mandelbrot | core | 381.5 | 382.6 | 381.4 | 1.00x | 1.00x | 172812923 |
| bench_class | core | 231.9 | 228.3 | 224.6 | 1.02x | 1.03x | 67884192 |
| bench_str_append | stdlib | 49.9 | 241.3 | 241.1 | 0.21x | 0.21x | 8976000000 |
| bench_vec_push | stdlib | 45.0 | 136.9 | 138.0 | 0.33x | 0.33x | 1535999952000000 |
| bench_hashmap_std | stdlib | 329.0 | 1554.0 | 1406.0 | 0.21x | 0.23x | 3999998000000 |
| bench_deque | stdlib | 115.3 | 108.4 | 95.3 | 1.06x | 1.21x | 511999984000000 |
| bench_malloc_churn | alloc | 150.3 | 515.9 | 496.1 | 0.29x | 0.30x | 1524000000 |
| bench_empty | startup | 1.0 | 1.4 | 1.0 | 0.68x | 0.99x |  |

### Geometric mean by tier

| Tier | CDD/clang | CDD/gcc |
|---|---|---|
| core (10) | 0.95x | 1.03x |
| stdlib (4) | 0.35x | 0.37x |
| alloc (1) | 0.29x | 0.30x |
| startup (1) | 0.68x | 0.99x |

### Run-to-run stability (CV %, lower is better)

| Benchmark | CDD CV | clang CV | gcc CV |
|---|---|---|---|
| bench_fib | 16.8% | 18.7% | 30.5% |
| bench_sieve | 2.7% | 3.1% | 1.4% |
| bench_matmul | 1.0% | 1.8% | 0.4% |
| bench_nbody | 0.6% | 0.4% | 0.4% |
| bench_sort | 0.4% | 3.2% | 1.8% |
| bench_hashmap | 2.3% | 3.7% | 3.0% |
| bench_nqueens | 2.2% | 2.2% | 0.2% |
| bench_binary_search | 0.8% | 2.8% | 0.6% |
| bench_mandelbrot | 0.1% | 0.2% | 0.1% |
| bench_class | 1.6% | 1.2% | 2.3% |
| bench_str_append | 12.4% | 1.2% | 1.3% |
| bench_vec_push | 4.2% | 0.6% | 1.7% |
| bench_hashmap_std | 1.7% | 0.8% | 0.9% |
| bench_deque | 0.8% | 0.8% | 0.8% |
| bench_malloc_churn | 2.2% | 2.5% | 1.9% |
| bench_empty | 2.9% | 25.1% | 6.2% |

## Optimization-level matrix (runtime median ms)

2 warmup + 3 runs per cell. Rows are the same workload compiled with a different `-O` level.

| Benchmark | O | CDD | clang | gcc | CDD/clang | CDD/gcc |
|---|---|---|---|---|---|---|
| bench_fib | O0 | 373.5 | 459.5 | 410.4 | 0.81x | 0.91x |
| bench_fib | O1 | 183.5 | 219.8 | 336.7 | 0.84x | 0.55x |
| bench_fib | O2 | 241.5 | 228.3 | 125.8 | 1.06x | 1.92x |
| bench_fib | O3 | 241.0 | 228.7 | 122.1 | 1.05x | 1.97x |
| bench_matmul | O0 | 680.8 | 791.8 | 846.6 | 0.86x | 0.80x |
| bench_matmul | O1 | 63.6 | 123.7 | 132.0 | 0.51x | 0.48x |
| bench_matmul | O2 | 63.9 | 72.3 | 74.4 | 0.88x | 0.86x |
| bench_matmul | O3 | 63.6 | 72.2 | 66.7 | 0.88x | 0.95x |
| bench_nbody | O0 | 606.8 | 1136.5 | 1351.8 | 0.53x | 0.45x |
| bench_nbody | O1 | 284.3 | 276.8 | 279.8 | 1.03x | 1.02x |
| bench_nbody | O2 | 279.4 | 294.0 | 293.1 | 0.95x | 0.95x |
| bench_nbody | O3 | 292.3 | 281.9 | 302.8 | 1.04x | 0.97x |
| bench_hashmap | O0 | 503.3 | 668.5 | 545.5 | 0.75x | 0.92x |
| bench_hashmap | O1 | 209.5 | 298.2 | 325.2 | 0.70x | 0.64x |
| bench_hashmap | O2 | 207.8 | 290.8 | 282.8 | 0.71x | 0.74x |
| bench_hashmap | O3 | 208.0 | 292.4 | 273.9 | 0.71x | 0.76x |
| bench_nqueens | O0 | 813.6 | 919.8 | 856.6 | 0.88x | 0.95x |
| bench_nqueens | O1 | 394.1 | 405.1 | 360.9 | 0.97x | 1.09x |
| bench_nqueens | O2 | 422.8 | 427.8 | 344.3 | 0.99x | 1.23x |
| bench_nqueens | O3 | 343.9 | 300.1 | 332.3 | 1.15x | 1.04x |
| bench_str_append | O0 | 141.8 | 2410.1 | 2172.5 | 0.06x | 0.07x |
| bench_str_append | O1 | 47.0 | 254.0 | 253.0 | 0.19x | 0.19x |
| bench_str_append | O2 | 52.5 | 244.0 | 244.4 | 0.22x | 0.21x |
| bench_str_append | O3 | 49.3 | 244.3 | 246.2 | 0.20x | 0.20x |

This matrix is diagnostic: it shows whether a gap is already present at `-O0` (front-end/codegen shape) or only appears after optimization.

## Compile time (median ms, lower is better)

| Source | CDD `-c` | CDD syntax | clang `-c` | gcc `-c` | clang syntax | gcc syntax |
|---|---|---|---|---|---|---|
| bench_fib | 19.4 | 12.5 | 32.0 | 43.3 | 23.5 | 10.7 |
| bench_sieve | 23.2 | 12.7 | 42.8 | 35.2 | 28.9 | 14.6 |
| bench_matmul | 25.3 | 13.0 | 44.8 | 40.5 | 28.8 | 14.9 |
| bench_nbody | 32.2 | 13.7 | 175.0 | 142.1 | 151.0 | 93.3 |
| bench_sort | 30.5 | 13.5 | 51.7 | 48.6 | 29.3 | 15.0 |
| bench_hashmap | 23.3 | 13.0 | 42.8 | 41.3 | 28.9 | 15.0 |
| bench_nqueens | 24.3 | 12.9 | 43.0 | 36.3 | 29.3 | 14.8 |
| bench_binary_search | 22.8 | 12.9 | 42.8 | 34.9 | 29.1 | 14.7 |
| bench_mandelbrot | 20.7 | 12.6 | 34.5 | 27.5 | 24.2 | 10.8 |
| bench_class | 27.0 | 13.0 | 46.7 | 39.6 | 29.4 | 14.9 |
| bench_str_append | 22.6 | 12.9 | 190.8 | 151.0 | 160.6 | 105.9 |
| bench_vec_push | 58.8 | 15.1 | 155.2 | 113.4 | 124.3 | 75.4 |
| bench_hashmap_std | 85.2 | 16.2 | 213.4 | 178.6 | 151.6 | 97.4 |
| bench_deque | 58.7 | 15.2 | 165.4 | 136.8 | 115.9 | 71.2 |
| bench_malloc_churn | 21.3 | 13.0 | 40.8 | 33.6 | 29.1 | 14.6 |
| bench_empty | 15.6 | 11.8 | 20.9 | 12.7 | 16.8 | 6.2 |
| corpus_100 | 88.3 | 16.5 | 124.4 | 192.3 | 24.4 | 9.6 |
| corpus_300 | 227.1 | 22.1 | 335.2 | 558.1 | 35.4 | 14.8 |
| corpus_600 | 443.2 | 32.1 | 682.6 | 1137.3 | 51.3 | 23.1 |
| corpus_1200 | 880.0 | 54.4 | 1456.8 | 2415.8 | 84.9 | 39.4 |

`syntax` is CDD's `-fsyntax-only` (lexer/parser/imports, no semantic analysis) versus the reference's `-fsyntax-only` (which does include semantic analysis), so the syntax columns are diagnostic, not a fair cross-language ratio.

### Compile scaling (synthetic corpus)

| Functions | CDD `-c` | clang `-c` | gcc `-c` | CDD ms/fn | clang ms/fn | gcc ms/fn |
|---|---|---|---|---|---|
| 100 | 88.3 | 124.4 | 192.3 | 0.88 | 1.24 | 1.92 |
| 300 | 227.1 | 335.2 | 558.1 | 0.76 | 1.12 | 1.86 |
| 600 | 443.2 | 682.6 | 1137.3 | 0.74 | 1.14 | 1.90 |
| 1200 | 880.0 | 1456.8 | 2415.8 | 0.73 | 1.21 | 2.01 |

`ms/fn` normalizes the fixed compiler startup cost and shows the marginal cost of one generated function.

## Code size: object sections (KiB)

| Benchmark | CDD `.text` | clang `.text` | gcc `.text` | CDD `.rodata` | CDD `.data` | CDD `.bss` | CDD debug |
|---|---|---|---|---|---|---|---|
| bench_fib | 0.1 | 0.1 | 0.9 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_sieve | 0.3 | 0.3 | 0.3 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_matmul | 0.6 | 0.6 | 0.7 | 0.1 | 0.0 | 0.0 | 0.5 |
| bench_nbody | 1.9 | 1.3 | 1.5 | 0.1 | 0.0 | 0.0 | 0.8 |
| bench_sort | 0.7 | 0.7 | 0.7 | 0.0 | 0.0 | 0.0 | 0.8 |
| bench_hashmap | 0.3 | 0.3 | 0.5 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_nqueens | 0.3 | 0.2 | 0.2 | 0.0 | 0.0 | 0.0 | 0.8 |
| bench_binary_search | 0.3 | 0.3 | 0.2 | 0.1 | 0.0 | 0.0 | 0.5 |
| bench_mandelbrot | 0.3 | 0.3 | 0.3 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_class | 0.5 | 0.4 | 0.4 | 0.0 | 0.0 | 0.0 | 0.6 |
| bench_str_append | 0.3 | 0.4 | 0.6 | 0.0 | 0.0 | 0.0 | 0.5 |
| bench_vec_push | 2.4 | 0.4 | 0.5 | 0.0 | 0.0 | 0.0 | 3.4 |
| bench_hashmap_std | 5.0 | 1.1 | 1.3 | 0.0 | 0.0 | 0.0 | 4.7 |
| bench_deque | 2.5 | 1.5 | 1.5 | 0.0 | 0.0 | 0.0 | 3.3 |
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
| bench_class | 259.2 | 222.3 | 25.6 | 14.2 | 25.2 | 14.1 | 10.14x |
| bench_str_append | 255.0 | 218.3 | 60.0 | 14.3 | 65.4 | 14.2 | 4.25x |
| bench_vec_push | 268.7 | 226.3 | 43.8 | 14.3 | 53.6 | 14.2 | 6.14x |
| bench_hashmap_std | 274.8 | 230.3 | 106.7 | 14.3 | 127.0 | 14.2 | 2.57x |
| bench_deque | 268.5 | 226.3 | 53.2 | 14.3 | 68.2 | 14.2 | 5.05x |
| bench_malloc_churn | 255.1 | 218.3 | 24.5 | 14.2 | 24.3 | 14.1 | 10.41x |
| bench_empty | 16.4 | 14.0 | 16.5 | 14.1 | 16.6 | 14.0 | 0.99x |

CDD links mimalloc statically, so its executables are much larger than the C++ references even when generated code is comparable. `stripped` removes symbols/debug information from a copy.

## LLVM IR size (diagnostic)

| Source | CDD bytes | CDD lines | clang bytes | clang lines | CDD/clang bytes |
|---|---|---|---|---|---|
| bench_fib | 2598 | 55 | 38693 | 427 | 0.07x |
| bench_sieve | 6548 | 119 | 63323 | 738 | 0.10x |
| bench_matmul | 8322 | 143 | 67026 | 789 | 0.12x |
| bench_nbody | 25010 | 392 | 137391 | 1553 | 0.18x |
| bench_sort | 17216 | 313 | 79927 | 1021 | 0.22x |
| bench_hashmap | 8413 | 153 | 69092 | 836 | 0.12x |
| bench_nqueens | 8331 | 157 | 64928 | 748 | 0.13x |
| bench_binary_search | 5692 | 106 | 63087 | 737 | 0.09x |
| bench_mandelbrot | 5005 | 99 | 46184 | 560 | 0.11x |
| bench_class | 13774 | 242 | 74112 | 877 | 0.19x |
| bench_str_append | 8178 | 135 | 222822 | 2166 | 0.04x |
| bench_vec_push | 62459 | 1175 | 161109 | 1704 | 0.39x |
| bench_hashmap_std | 107107 | 1947 | 409521 | 3952 | 0.26x |
| bench_deque | 61098 | 1143 | 250127 | 2910 | 0.24x |
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
| bench_nbody | 4.1 | 4.0 | 1.7 | 1.02x |
| bench_sort | 34.0 | 34.3 | 32.2 | 0.99x |
| bench_hashmap | 166.1 | 162.3 | 160.3 | 1.02x |
| bench_nqueens | 4.0 | 4.0 | 1.7 | 1.01x |
| bench_binary_search | 70.0 | 67.8 | 65.7 | 1.03x |
| bench_mandelbrot | 1.7 | 4.1 | 1.7 | 0.41x |
| bench_class | 50.0 | 49.5 | 47.5 | 1.01x |
| bench_str_append | 185.8 | 243.6 | 243.7 | 0.76x |
| bench_vec_push | 181.8 | 131.6 | 131.6 | 1.38x |
| bench_hashmap_std | 166.1 | 186.7 | 186.7 | 0.89x |
| bench_deque | 179.7 | 131.6 | 131.6 | 1.37x |
| bench_malloc_churn | 418.1 | 507.4 | 505.2 | 0.82x |
| bench_empty | 1.4 | 3.8 | 1.4 | 0.37x |

Peak RSS is measured once per implementation and includes the static mimalloc arena for CDD; allocator caching can make it sensitive to run order.

### Peak memory: compile (MiB)

| Source | CDD | clang | gcc |
|---|---|---|---|
| bench_fib | 86.2 | 104.7 | 31.4 |
| bench_sieve | 85.9 | 106.5 | 32.5 |
| bench_matmul | 92.3 | 106.8 | 34.6 |
| bench_nbody | 91.2 | 120.5 | 64.4 |
| bench_sort | 89.9 | 107.3 | 33.9 |
| bench_hashmap | 86.9 | 105.8 | 32.7 |
| bench_nqueens | 86.5 | 106.4 | 31.8 |
| bench_binary_search | 89.3 | 106.6 | 32.3 |
| bench_mandelbrot | 88.6 | 105.2 | 30.4 |
| bench_class | 91.2 | 107.5 | 33.6 |
| bench_str_append | 87.2 | 121.7 | 67.9 |
| bench_vec_push | 94.3 | 118.8 | 56.3 |
| bench_hashmap_std | 97.5 | 122.8 | 68.5 |
| bench_deque | 95.5 | 118.1 | 57.6 |
| bench_malloc_churn | 86.4 | 105.7 | 31.8 |
| bench_empty | 81.4 | 96.3 | 23.4 |
| corpus_100 | 91.3 | 104.0 | 35.3 |
| corpus_300 | 105.2 | 111.4 | 52.4 |
| corpus_600 | 125.2 | 123.0 | 75.3 |
| corpus_1200 | 157.3 | 146.3 | 121.5 |

## Hardware counters (one representative run)

| Benchmark | Impl | cycles (M) | instructions (M) | IPC | GHz | cache-miss % | branch-miss % |
|---|---|---|---|---|---|---|---|
| bench_fib | cdd | 1116.5 | 3648.2 | 3.27 | 4.62 | 0.00 | 0.21 |
| bench_fib | clang | 1056.3 | 3650.7 | 3.46 | 4.61 | 0.00 | 0.21 |
| bench_fib | gcc | 1009.3 | 2764.8 | 2.74 | 4.57 | 0.01 | 0.45 |
| bench_sieve | cdd | 4874.9 | 2252.6 | 0.46 | 4.65 | 6.10 | 0.14 |
| bench_sieve | clang | 5606.3 | 2527.4 | 0.45 | 4.65 | 5.84 | 1.22 |
| bench_sieve | gcc | 5621.4 | 2501.0 | 0.44 | 4.64 | 5.93 | 1.22 |
| bench_matmul | cdd | 279.2 | 1322.3 | 4.74 | 4.56 | 0.11 | 0.12 |
| bench_matmul | clang | 320.1 | 1343.2 | 4.20 | 4.58 | 0.14 | 0.84 |
| bench_matmul | gcc | 331.2 | 1849.7 | 5.59 | 4.58 | 0.11 | 0.47 |
| bench_nbody | cdd | 1288.4 | 3261.6 | 2.53 | 4.64 | 0.00 | 0.13 |
| bench_nbody | clang | 1360.4 | 3013.9 | 2.22 | 4.64 | 0.00 | 0.15 |
| bench_nbody | gcc | 1356.6 | 2951.6 | 2.18 | 4.64 | 0.00 | 0.20 |
| bench_sort | cdd | 2147.4 | 2048.2 | 0.95 | 4.64 | 0.04 | 18.50 |
| bench_sort | clang | 2072.4 | 1959.8 | 0.95 | 4.64 | 0.05 | 20.09 |
| bench_sort | gcc | 2107.4 | 2189.8 | 1.04 | 4.64 | 0.05 | 20.04 |
| bench_hashmap | cdd | 962.8 | 355.7 | 0.37 | 4.64 | 8.28 | 0.28 |
| bench_hashmap | clang | 1291.8 | 582.6 | 0.45 | 4.64 | 6.73 | 4.26 |
| bench_hashmap | gcc | 1337.4 | 543.1 | 0.41 | 4.64 | 7.21 | 4.89 |
| bench_nqueens | cdd | 1902.3 | 4054.4 | 2.13 | 4.64 | 0.00 | 2.94 |
| bench_nqueens | clang | 1892.4 | 4027.1 | 2.13 | 4.64 | 0.00 | 2.95 |
| bench_nqueens | gcc | 1596.0 | 4126.6 | 2.59 | 4.64 | 0.00 | 2.91 |
| bench_binary_search | cdd | 3258.5 | 571.0 | 0.18 | 4.65 | 34.53 | 17.25 |
| bench_binary_search | clang | 3092.9 | 662.6 | 0.21 | 4.65 | 18.66 | 16.54 |
| bench_binary_search | gcc | 3060.1 | 576.9 | 0.19 | 4.64 | 20.11 | 16.38 |
| bench_mandelbrot | cdd | 1768.6 | 2707.3 | 1.53 | 4.65 | 0.00 | 0.17 |
| bench_mandelbrot | clang | 1769.8 | 2709.4 | 1.53 | 4.64 | 0.00 | 0.18 |
| bench_mandelbrot | gcc | 1766.9 | 2787.0 | 1.58 | 4.64 | 0.00 | 0.13 |
| bench_class | cdd | 1056.3 | 3936.3 | 3.73 | 4.60 | 0.03 | 0.07 |
| bench_class | clang | 1048.6 | 3248.6 | 3.10 | 4.64 | 0.05 | 0.60 |
| bench_class | gcc | 1036.5 | 4248.7 | 4.10 | 4.62 | 0.04 | 0.59 |
| bench_str_append | cdd | 254.0 | 460.1 | 1.81 | 4.63 | 0.22 | 0.54 |
| bench_str_append | clang | 1145.2 | 1302.3 | 1.14 | 4.65 | 0.41 | 5.02 |
| bench_str_append | gcc | 1111.5 | 1405.4 | 1.26 | 4.64 | 0.40 | 5.20 |
| bench_vec_push | cdd | 203.9 | 546.2 | 2.68 | 4.63 | 0.12 | 0.23 |
| bench_vec_push | clang | 623.2 | 709.1 | 1.14 | 4.64 | 0.47 | 5.57 |
| bench_vec_push | gcc | 631.0 | 765.0 | 1.21 | 4.64 | 0.44 | 4.79 |
| bench_hashmap_std | cdd | 1492.4 | 1332.0 | 0.89 | 4.64 | 1.75 | 4.03 |
| bench_hashmap_std | clang | 7144.4 | 2455.9 | 0.34 | 4.65 | 2.55 | 3.23 |
| bench_hashmap_std | gcc | 6551.3 | 2355.9 | 0.36 | 4.64 | 2.65 | 3.39 |
| bench_deque | cdd | 528.3 | 1261.7 | 2.39 | 4.63 | 0.05 | 0.08 |
| bench_deque | clang | 495.0 | 1040.9 | 2.10 | 4.64 | 0.18 | 2.27 |
| bench_deque | gcc | 447.2 | 845.2 | 1.89 | 4.64 | 0.21 | 2.64 |
| bench_malloc_churn | cdd | 713.3 | 541.9 | 0.76 | 4.62 | 4.72 | 0.48 |
| bench_malloc_churn | clang | 2338.3 | 2833.0 | 1.21 | 4.64 | 1.68 | 2.43 |
| bench_malloc_churn | gcc | 2117.7 | 2851.3 | 1.35 | 4.64 | 1.68 | 2.42 |
| bench_empty | cdd | 1.1 | 0.8 | 0.71 | 3.39 | 2.90 | 12.35 |
| bench_empty | clang | 2.9 | 3.2 | 1.09 | 3.61 | 1.49 | 7.58 |
| bench_empty | gcc | 1.1 | 0.8 | 0.72 | 3.34 | 2.99 | 12.36 |

## Analysis and optimization targets

- Largest core gaps vs clang: `bench_fib` 1.06x, `bench_binary_search` 1.05x, `bench_sort` 1.03x.
- Largest core wins vs clang: `bench_matmul` 0.88x, `bench_sieve` 0.87x, `bench_hashmap` 0.68x.
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

