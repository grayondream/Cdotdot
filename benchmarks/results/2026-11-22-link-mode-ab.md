# Link mode — PIE vs non-PIE A/B

Date: `2026-11-22`

This is a same-object A/B for the executable link mode, not a new absolute
baseline. The host was running an external Java workload, so absolute times
are noisy; the old/new binaries were alternated on the same pinned core.

## Result

Same CDD objects, only the link mode changed:

| Benchmark | PIE (ms) | non-PIE (ms) | non-PIE/PIE |
|---|---:|---:|---:|
| bench_fib | 247.49 | 223.06 | **0.901** |
| bench_binary_search | 813.09 | 802.63 | 0.987 |
| bench_sieve | 1298.46 | 1274.25 | 0.981 |
| bench_matmul | 39.53 | 39.03 | 0.987 |
| bench_hashmap | 221.40 | 217.73 | 0.983 |
| bench_vec_push | 55.60 | 51.17 | 0.920 |
| bench_nbody | 277.61 | 278.29 | 1.002 |
| bench_nqueens | 347.47 | 348.12 | 1.002 |
| bench_sort | 460.50 | 461.04 | 1.001 |
| bench_class | 195.05 | 196.65 | 1.008 |
| bench_malloc_churn | 170.10 | 173.23 | 1.018 |
| bench_deque | 65.05 | 65.44 | 1.006 |
| bench_hashmap_std | 356.09 | 355.06 | 0.997 |
| bench_mandelbrot | 379.85 | 379.64 | 0.999 |
| bench_empty | 0.96 | 0.94 | 0.978 |
| bench_str_append | 42.32 | 41.68 | 0.985 |

- **core geomean: 0.9849x**
- **all 16 workloads geomean: 0.9844x**
- Checksums all match.

## Why it is not the default

The win is not a code-generation difference. Function bytes and relative
offsets are identical; the linker places the PIE `.text` at `0x1050` and the
non-PIE `.text` at `0x1040`, shifting code addresses by `0x10`:

```text
PIE:     fib_int32 @ 0x1140
non-PIE: fib_int32 @ 0x401130
```

`fib` is particularly sensitive to that address (about 9%); most other rows
are within layout/noise. Non-PIE also disables ASLR for the main executable.

The PIE-safe alternatives tested and rejected:

- Global function alignment to 64B: core geomean 1.004x; class/matmul regress.
- `-Wl,-z,noseparate-code`: preserves ASLR but merges the code segment (breaks
  W^X); core 0.980x, but class/binary/hashmap_std regress.
- `-Wl,-Ttext=0x10c0`: fixes small programs like fib, but large programs fail
  to link; not general.

## Decision

The default remains **PIE**. `-no-pie` is available as an explicit performance
option; `-pie` explicitly selects the default. This keeps the default ASLR and
W^X behavior, while allowing the layout experiment where needed.
