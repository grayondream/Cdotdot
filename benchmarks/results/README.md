# Baseline results

| File | Scope | Use |
|---|---|---|
| `2026-11-18-fixed.{json,md}` | 16 workloads, 5 warmup + 9 runs, fixed `-O0` matrix, RSS/IR/object/stripped sizes, corpus 100–1200 | **Current baseline**; compare future work against this file |
| `2026-11-17-baseline.{json,md}` | Same scope, taken before the 2026-11-18 syntax/codegen fixes; its O0 matrix contains error cells | Historical comparison only |
| `2026-11-16-baseline.{json,md}` | 9 workloads, 2 warmup + 7 runs, no `-g` on C++ references, no matrix/RSS/IR | Historical first pass |

The 2026-11-18 run is the current baseline. Do not mix compile-time or size
numbers across methodology changes. Runtime ratios may be compared
descriptively, with the machine load and CV table in mind.

`benchmarks/BASELINE.md` is the rendered copy of the current baseline.
