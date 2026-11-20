# CDD Performance Baseline — 2026-11-20-simd

Generated: `2026-11-20T22:00:05`
Command: `/home/ares/apps/conda/bin/python3 benchmarks/run.py --full --baseline-md --label 2026-11-20-simd`

## Environment

| Item | Value |
|---|---|
| CPU | AMD Ryzen 5 5600X 6-Core Processor |
| CPU count / pinned | 12 / 10 |
| Governor | powersave |
| Load average (1/5/15 min) | 2.29 / 0.89 / 0.63 |
| Load average at end | 1.52 / 1.4 / 1.07 |
| Selected core busy at start | 0.0% |
| Kernel | 7.0.0-43-generic (x86_64) |
| CDD | `/media/nvm1/workspace/Cdotdot/build/bin/cdd` (3313640 bytes, build type `Release`) |
| CDD commit | `a533c95` master (dirty worktree) |
| LLVM (llvm-config) | 21.1.8 |
| clang | Ubuntu clang version 22.1.2 (1ubuntu1) |
| gcc | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |

Method: `-O2` for every compiler, 5 warmup + 9 measured runs (median) for runtime, 1 warmup + 5 runs for compile time, one implementation round at a time (interleaved), process pinned with `taskset`. Each row runs the same algorithm, data types and workload size in CDD and C++. C++ references use `-g` because CDD emits debug information by default. The report also records peak RSS, LLVM IR size, object-section breakdown, stripped executable size, and an optional `-O0..-O3` matrix.

## Headline

- `core` geometric mean: **0.95x** vs clang, **1.01x** vs gcc
- `stdlib` geometric mean: **0.35x** vs clang, **0.37x** vs gcc
- `alloc` geometric mean: **0.30x** vs clang, **0.30x** vs gcc
- `startup` geometric mean: **0.68x** vs clang, **0.98x** vs gcc
- Largest core gap vs clang: `bench_fib` at **1.06x**; largest core win: `bench_hashmap` at **0.72x**.

## Runtime (median, lower is better; ratio = CDD / reference)

| Benchmark | Tier | CDD (ms) | clang (ms) | gcc (ms) | CDD/clang | CDD/gcc | checksum |
|---|---|---|---|---|---|---|---|
| bench_fib | core | 240.7 | 227.7 | 124.8 | 1.06x | 1.93x | 102334155 |
| bench_sieve | core | 1063.9 | 1221.7 | 1221.3 | 0.87x | 0.87x | 11078937 |
| bench_matmul | core | 62.6 | 71.2 | 72.3 | 0.88x | 0.87x | 2876 |
| bench_nbody | core | 277.0 | 291.9 | 291.0 | 0.95x | 0.95x | 1690136770432667 |
| bench_sort | core | 458.4 | 443.3 | 449.3 | 1.03x | 1.02x | 8588201207136256 |
| bench_hashmap | core | 207.5 | 286.5 | 298.5 | 0.72x | 0.69x | 4000000 |
| bench_nqueens | core | 366.0 | 361.4 | 341.8 | 1.01x | 1.07x | 73712 |
| bench_binary_search | core | 704.2 | 695.6 | 684.1 | 1.01x | 1.03x | 8382020676000 |
| bench_mandelbrot | core | 378.2 | 378.9 | 377.9 | 1.00x | 1.00x | 172812923 |
| bench_class | core | 233.8 | 234.1 | 231.2 | 1.00x | 1.01x | 67884192 |
| bench_str_append | stdlib | 52.0 | 241.6 | 242.9 | 0.22x | 0.21x | 8976000000 |
| bench_vec_push | stdlib | 47.1 | 138.7 | 139.7 | 0.34x | 0.34x | 1535999952000000 |
| bench_hashmap_std | stdlib | 334.7 | 1628.4 | 1462.0 | 0.21x | 0.23x | 3999998000000 |
| bench_deque | stdlib | 114.0 | 108.9 | 95.5 | 1.05x | 1.19x | 511999984000000 |
| bench_malloc_churn | alloc | 150.9 | 503.4 | 506.7 | 0.30x | 0.30x | 1524000000 |
| bench_empty | startup | 1.0 | 1.5 | 1.0 | 0.68x | 0.98x |  |

### Geometric mean by tier

| Tier | CDD/clang | CDD/gcc |
|---|---|---|
| core (10) | 0.95x | 1.01x |
| stdlib (4) | 0.35x | 0.37x |
| alloc (1) | 0.30x | 0.30x |
| startup (1) | 0.68x | 0.98x |

### Run-to-run stability (CV %, lower is better)

| Benchmark | CDD CV | clang CV | gcc CV |
|---|---|---|---|
| bench_fib | 0.5% | 0.4% | 0.4% |
| bench_sieve | 0.2% | 0.4% | 0.5% |
| bench_matmul | 3.0% | 4.7% | 0.4% |
| bench_nbody | 0.3% | 0.1% | 0.1% |
| bench_sort | 0.2% | 0.4% | 0.1% |
| bench_hashmap | 3.1% | 3.4% | 2.0% |
| bench_nqueens | 4.4% | 2.5% | 0.9% |
| bench_binary_search | 0.8% | 2.1% | 1.1% |
| bench_mandelbrot | 0.0% | 0.1% | 0.0% |
| bench_class | 2.8% | 2.7% | 2.3% |
| bench_str_append | 11.4% | 0.9% | 0.7% |
| bench_vec_push | 7.3% | 1.4% | 0.6% |
| bench_hashmap_std | 1.8% | 0.7% | 0.8% |
| bench_deque | 0.6% | 0.6% | 0.3% |
| bench_malloc_churn | 2.3% | 0.8% | 0.7% |
| bench_empty | 2.4% | 2.8% | 2.4% |

## Optimization-level matrix (runtime median ms)

2 warmup + 3 runs per cell. Rows are the same workload compiled with a different `-O` level.

| Benchmark | O | CDD | clang | gcc | CDD/clang | CDD/gcc |
|---|---|---|---|---|---|---|
| bench_fib | O0 | 369.8 | 456.9 | 407.2 | 0.81x | 0.91x |
| bench_fib | O1 | 183.6 | 220.0 | 334.6 | 0.83x | 0.55x |
| bench_fib | O2 | 240.4 | 227.1 | 125.3 | 1.06x | 1.92x |
| bench_fib | O3 | 240.5 | 228.5 | 121.2 | 1.05x | 1.98x |
| bench_matmul | O0 | 666.5 | 778.9 | 828.5 | 0.86x | 0.80x |
| bench_matmul | O1 | 63.4 | 121.5 | 129.5 | 0.52x | 0.49x |
| bench_matmul | O2 | 62.9 | 74.2 | 76.9 | 0.85x | 0.82x |
| bench_matmul | O3 | 63.2 | 71.8 | 66.1 | 0.88x | 0.96x |
| bench_nbody | O0 | 601.3 | 1120.0 | 1339.2 | 0.54x | 0.45x |
| bench_nbody | O1 | 282.2 | 274.8 | 277.9 | 1.03x | 1.02x |
| bench_nbody | O2 | 278.7 | 292.7 | 290.5 | 0.95x | 0.96x |
| bench_nbody | O3 | 290.5 | 279.8 | 300.3 | 1.04x | 0.97x |
| bench_hashmap | O0 | 509.5 | 676.2 | 567.8 | 0.75x | 0.90x |
| bench_hashmap | O1 | 209.7 | 294.4 | 334.4 | 0.71x | 0.63x |
| bench_hashmap | O2 | 210.2 | 296.7 | 278.1 | 0.71x | 0.76x |
| bench_hashmap | O3 | 209.9 | 296.2 | 287.0 | 0.71x | 0.73x |
| bench_nqueens | O0 | 807.3 | 911.6 | 852.6 | 0.89x | 0.95x |
| bench_nqueens | O1 | 407.1 | 393.2 | 357.9 | 1.04x | 1.14x |
| bench_nqueens | O2 | 372.6 | 350.6 | 341.2 | 1.06x | 1.09x |
| bench_nqueens | O3 | 345.2 | 297.3 | 331.0 | 1.16x | 1.04x |
| bench_str_append | O0 | 147.8 | 2396.9 | 2164.3 | 0.06x | 0.07x |
| bench_str_append | O1 | 47.7 | 252.0 | 251.3 | 0.19x | 0.19x |
| bench_str_append | O2 | 47.7 | 244.5 | 242.5 | 0.20x | 0.20x |
| bench_str_append | O3 | 48.1 | 243.2 | 242.8 | 0.20x | 0.20x |

This matrix is diagnostic: it shows whether a gap is already present at `-O0` (front-end/codegen shape) or only appears after optimization.

## Compile time (median ms, lower is better)

| Source | CDD `-c` | CDD syntax | clang `-c` | gcc `-c` | clang syntax | gcc syntax |
|---|---|---|---|---|---|---|
| bench_fib | 19.0 | 12.2 | 34.5 | 43.6 | 24.1 | 11.1 |
| bench_sieve | 23.4 | 12.8 | 45.1 | 35.9 | 30.0 | 14.9 |
| bench_matmul | 25.2 | 12.6 | 47.0 | 41.1 | 30.1 | 15.1 |
| bench_nbody | 32.2 | 13.5 | 176.0 | 143.0 | 151.1 | 93.4 |
| bench_sort | 29.5 | 12.9 | 54.1 | 49.2 | 29.9 | 15.2 |
| bench_hashmap | 22.8 | 12.5 | 45.2 | 41.7 | 30.0 | 15.1 |
| bench_nqueens | 23.7 | 12.7 | 44.4 | 36.5 | 30.1 | 15.0 |
| bench_binary_search | 22.6 | 12.9 | 44.9 | 35.6 | 30.9 | 15.3 |
| bench_mandelbrot | 21.0 | 12.6 | 36.2 | 27.6 | 24.2 | 10.9 |
| bench_class | 26.2 | 13.0 | 48.8 | 40.1 | 30.3 | 15.1 |
| bench_str_append | 22.1 | 12.1 | 189.6 | 150.0 | 162.1 | 105.4 |
| bench_vec_push | 56.0 | 14.7 | 156.2 | 115.8 | 125.8 | 75.1 |
| bench_hashmap_std | 82.7 | 15.1 | 213.0 | 181.6 | 152.9 | 97.0 |
| bench_deque | 56.9 | 14.8 | 165.2 | 136.2 | 116.2 | 71.6 |
| bench_malloc_churn | 21.0 | 12.4 | 42.6 | 34.1 | 29.6 | 14.9 |
| bench_empty | 15.9 | 11.5 | 23.1 | 12.6 | 17.5 | 6.1 |
| corpus_100 | 87.3 | 16.1 | 126.1 | 192.0 | 24.9 | 9.8 |
| corpus_300 | 223.8 | 22.3 | 339.8 | 556.8 | 35.8 | 14.8 |
| corpus_600 | 434.3 | 32.6 | 676.8 | 1129.0 | 52.9 | 22.3 |
| corpus_1200 | 858.2 | 52.0 | 1440.9 | 2382.6 | 84.9 | 38.8 |

`syntax` is CDD's `-fsyntax-only` (lexer/parser/imports, no semantic analysis) versus the reference's `-fsyntax-only` (which does include semantic analysis), so the syntax columns are diagnostic, not a fair cross-language ratio.

### Compile scaling (synthetic corpus)

| Functions | CDD `-c` | clang `-c` | gcc `-c` | CDD ms/fn | clang ms/fn | gcc ms/fn |
|---|---|---|---|---|---|
| 100 | 87.3 | 126.1 | 192.0 | 0.87 | 1.26 | 1.92 |
| 300 | 223.8 | 339.8 | 556.8 | 0.75 | 1.13 | 1.86 |
| 600 | 434.3 | 676.8 | 1129.0 | 0.72 | 1.13 | 1.88 |
| 1200 | 858.2 | 1440.9 | 2382.6 | 0.72 | 1.20 | 1.99 |

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
| bench_hashmap_std | 5.1 | 1.1 | 1.3 | 0.0 | 0.0 | 0.0 | 4.7 |
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
| bench_hashmap_std | 274.9 | 230.3 | 106.7 | 14.3 | 127.0 | 14.2 | 2.58x |
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
| bench_nbody | 24944 | 392 | 137391 | 1553 | 0.18x |
| bench_sort | 17259 | 313 | 79927 | 1021 | 0.22x |
| bench_hashmap | 8413 | 153 | 69092 | 836 | 0.12x |
| bench_nqueens | 8255 | 157 | 64928 | 748 | 0.13x |
| bench_binary_search | 5692 | 106 | 63087 | 737 | 0.09x |
| bench_mandelbrot | 5005 | 99 | 46184 | 560 | 0.11x |
| bench_class | 13774 | 242 | 74112 | 877 | 0.19x |
| bench_str_append | 8389 | 138 | 222822 | 2166 | 0.04x |
| bench_vec_push | 61947 | 1174 | 161109 | 1704 | 0.38x |
| bench_hashmap_std | 106041 | 1931 | 409521 | 3952 | 0.26x |
| bench_deque | 60350 | 1137 | 250127 | 2910 | 0.24x |
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
| bench_fib | 1.7 | 4.0 | 1.7 | 0.42x |
| bench_sieve | 196.0 | 194.5 | 192.4 | 1.01x |
| bench_matmul | 18.1 | 17.3 | 15.2 | 1.04x |
| bench_nbody | 4.1 | 4.1 | 1.7 | 1.00x |
| bench_sort | 34.1 | 34.3 | 32.2 | 0.99x |
| bench_hashmap | 166.1 | 162.3 | 160.3 | 1.02x |
| bench_nqueens | 4.1 | 4.1 | 1.7 | 1.00x |
| bench_binary_search | 70.1 | 67.8 | 65.7 | 1.03x |
| bench_mandelbrot | 1.7 | 3.9 | 1.7 | 0.43x |
| bench_class | 50.1 | 49.6 | 47.6 | 1.01x |
| bench_str_append | 185.8 | 243.7 | 243.7 | 0.76x |
| bench_vec_push | 181.7 | 131.6 | 131.7 | 1.38x |
| bench_hashmap_std | 166.1 | 186.7 | 186.6 | 0.89x |
| bench_deque | 181.8 | 131.7 | 131.6 | 1.38x |
| bench_malloc_churn | 418.1 | 507.4 | 505.3 | 0.82x |
| bench_empty | 1.4 | 3.8 | 1.4 | 0.37x |

Peak RSS is measured once per implementation and includes the static mimalloc arena for CDD; allocator caching can make it sensitive to run order.

### Peak memory: compile (MiB)

| Source | CDD | clang | gcc |
|---|---|---|---|
| bench_fib | 79.3 | 104.6 | 31.2 |
| bench_sieve | 80.7 | 106.7 | 32.5 |
| bench_matmul | 87.0 | 106.9 | 34.9 |
| bench_nbody | 88.1 | 120.5 | 64.3 |
| bench_sort | 82.6 | 107.4 | 33.9 |
| bench_hashmap | 79.7 | 105.8 | 32.6 |
| bench_nqueens | 81.8 | 106.3 | 31.7 |
| bench_binary_search | 86.4 | 106.7 | 32.4 |
| bench_mandelbrot | 85.9 | 105.2 | 30.7 |
| bench_class | 84.1 | 107.5 | 33.8 |
| bench_str_append | 82.1 | 121.7 | 67.8 |
| bench_vec_push | 85.2 | 118.9 | 56.5 |
| bench_hashmap_std | 88.6 | 122.7 | 68.5 |
| bench_deque | 88.7 | 118.0 | 57.8 |
| bench_malloc_churn | 81.5 | 105.7 | 31.8 |
| bench_empty | 76.6 | 96.3 | 23.4 |
| corpus_100 | 88.5 | 104.0 | 35.2 |
| corpus_300 | 100.6 | 111.3 | 52.3 |
| corpus_600 | 122.5 | 123.0 | 75.1 |
| corpus_1200 | 156.7 | 146.4 | 121.4 |

## Hardware counters (one representative run)

| Benchmark | Impl | cycles (M) | instructions (M) | IPC | GHz | cache-miss % | branch-miss % |
|---|---|---|---|---|---|---|---|
| bench_fib | cdd | 1105.8 | 3646.0 | 3.30 | 4.64 | 0.00 | 0.20 |
| bench_fib | clang | 1044.6 | 3648.3 | 3.49 | 4.65 | 0.00 | 0.20 |
| bench_fib | gcc | 573.7 | 2761.8 | 4.81 | 4.65 | 0.00 | 0.41 |
| bench_sieve | cdd | 4967.4 | 2241.3 | 0.45 | 4.65 | 6.06 | 0.08 |
| bench_sieve | clang | 5687.4 | 2515.4 | 0.44 | 4.65 | 5.90 | 1.16 |
| bench_sieve | gcc | 5700.9 | 2486.3 | 0.44 | 4.65 | 5.97 | 1.16 |
| bench_matmul | cdd | 277.9 | 1321.7 | 4.76 | 4.51 | 0.10 | 0.10 |
| bench_matmul | clang | 318.9 | 1342.8 | 4.21 | 4.52 | 0.14 | 0.82 |
| bench_matmul | gcc | 329.1 | 1849.1 | 5.62 | 4.58 | 0.10 | 0.47 |
| bench_nbody | cdd | 1277.7 | 3259.2 | 2.55 | 4.65 | 0.00 | 0.08 |
| bench_nbody | clang | 1349.9 | 3011.2 | 2.23 | 4.65 | 0.00 | 0.10 |
| bench_nbody | gcc | 1344.3 | 2948.8 | 2.19 | 4.65 | 0.00 | 0.12 |
| bench_sort | cdd | 2123.3 | 2044.2 | 0.96 | 4.65 | 0.04 | 18.53 |
| bench_sort | clang | 2053.1 | 1955.0 | 0.95 | 4.65 | 0.05 | 20.13 |
| bench_sort | gcc | 2082.8 | 2185.6 | 1.05 | 4.65 | 0.05 | 20.07 |
| bench_hashmap | cdd | 975.4 | 353.0 | 0.36 | 4.65 | 8.31 | 0.20 |
| bench_hashmap | clang | 1348.5 | 579.9 | 0.43 | 4.65 | 6.85 | 4.22 |
| bench_hashmap | gcc | 1273.5 | 540.7 | 0.42 | 4.65 | 7.36 | 4.84 |
| bench_nqueens | cdd | 1653.0 | 4050.0 | 2.45 | 4.65 | 0.00 | 2.95 |
| bench_nqueens | clang | 1692.7 | 4023.0 | 2.38 | 4.65 | 0.00 | 2.96 |
| bench_nqueens | gcc | 1590.3 | 4123.5 | 2.59 | 4.65 | 0.00 | 2.90 |
| bench_binary_search | cdd | 3312.4 | 563.2 | 0.17 | 4.65 | 35.50 | 17.32 |
| bench_binary_search | clang | 3333.7 | 654.9 | 0.20 | 4.65 | 18.62 | 16.59 |
| bench_binary_search | gcc | 3330.8 | 569.8 | 0.17 | 4.65 | 20.47 | 16.41 |
| bench_mandelbrot | cdd | 1753.1 | 2703.6 | 1.54 | 4.65 | 0.00 | 0.14 |
| bench_mandelbrot | clang | 1754.3 | 2706.1 | 1.54 | 4.65 | 0.00 | 0.15 |
| bench_mandelbrot | gcc | 1751.0 | 2783.5 | 1.59 | 4.65 | 0.00 | 0.11 |
| bench_class | cdd | 1077.5 | 3933.8 | 3.65 | 4.60 | 0.03 | 0.05 |
| bench_class | clang | 1074.8 | 3246.8 | 3.02 | 4.64 | 0.05 | 0.58 |
| bench_class | gcc | 1059.8 | 4246.4 | 4.01 | 4.62 | 0.04 | 0.57 |
| bench_str_append | cdd | 252.6 | 459.3 | 1.82 | 4.63 | 0.16 | 0.46 |
| bench_str_append | clang | 1124.3 | 1303.5 | 1.16 | 4.65 | 0.40 | 4.99 |
| bench_str_append | gcc | 1154.6 | 1408.6 | 1.22 | 4.65 | 0.36 | 5.17 |
| bench_vec_push | cdd | 193.0 | 545.7 | 2.83 | 4.63 | 0.12 | 0.21 |
| bench_vec_push | clang | 651.3 | 709.7 | 1.09 | 4.65 | 0.47 | 5.53 |
| bench_vec_push | gcc | 636.3 | 765.6 | 1.20 | 4.65 | 0.44 | 4.76 |
| bench_hashmap_std | cdd | 1565.5 | 1345.7 | 0.86 | 4.65 | 1.75 | 4.17 |
| bench_hashmap_std | clang | 7483.1 | 2444.3 | 0.33 | 4.65 | 2.56 | 3.19 |
| bench_hashmap_std | gcc | 6782.9 | 2342.5 | 0.35 | 4.65 | 2.65 | 3.33 |
| bench_deque | cdd | 522.9 | 1228.5 | 2.35 | 4.65 | 0.04 | 0.07 |
| bench_deque | clang | 493.2 | 1041.1 | 2.11 | 4.65 | 0.16 | 2.26 |
| bench_deque | gcc | 435.0 | 845.2 | 1.94 | 4.65 | 0.19 | 2.63 |
| bench_malloc_churn | cdd | 691.3 | 540.9 | 0.78 | 4.65 | 4.80 | 0.44 |
| bench_malloc_churn | clang | 2153.3 | 2830.4 | 1.31 | 4.65 | 1.63 | 2.39 |
| bench_malloc_churn | gcc | 2160.4 | 2850.6 | 1.32 | 4.65 | 1.64 | 2.39 |
| bench_empty | cdd | 1.2 | 0.8 | 0.67 | 4.30 | 3.44 | 12.55 |
| bench_empty | clang | 2.8 | 3.2 | 1.11 | 4.44 | 1.50 | 7.52 |
| bench_empty | gcc | 1.1 | 0.8 | 0.71 | 4.33 | 3.00 | 12.32 |

## Analysis and optimization targets

- Largest core gaps vs clang: `bench_fib` 1.06x, `bench_sort` 1.03x, `bench_nqueens` 1.01x.
- Largest core wins vs clang: `bench_matmul` 0.88x, `bench_sieve` 0.87x, `bench_hashmap` 0.72x.
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

