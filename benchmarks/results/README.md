# Baseline results

| File | Scope | Use |
|---|---|---|
| `2026-11-17-baseline.{json,md}` | 16 workloads, 5 warmup + 9 runs, `-O0..-O3` matrix, RSS/IR/object/stripped sizes, corpus 100–1200 | **Current baseline**; compare future work against this file |
| `2026-11-16-baseline.{json,md}` | 9 workloads, 2 warmup + 7 runs, no `-g` on C++ references, no matrix/RSS/IR | Historical first pass |

The 2026-11-17 run changed methodology (C++ references use `-g`, more
workloads, more repetitions), so do not mix compile-time or size numbers
across the two files. Runtime ratios may be compared descriptively.

`benchmarks/BASELINE.md` is the rendered copy of the current baseline.
