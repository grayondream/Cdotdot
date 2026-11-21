# LLVM 22 与 IR 元数据优化设计

## 目标

1. 将本地构建工具链切到 LLVM 22，并保持现有 18+ 兼容代码路径。
2. 在代码生成后、优化前补充低风险 IR 元数据，争取改善 sort/hashmap/nbody 等核心负载。
3. 用现有 benchmark 方法验证收益；没有收益就回退元数据改动，不把“更多元数据”本身当成成果。

## 非目标

- 不默认启用 `-O3`、fast-math、PGO、BOLT。
- 不改变默认 `generic`/SSE2 CPU 策略。
- 不实现 TBAA。CDD 的严格别名语义尚未明确，且 `reinterpret_cast` 允许指针重解释；在契约明确前加 TBAA 有误编译风险。
- 不改变语言运行时语义。

## 工具链决策

- 构建脚本优先使用已安装的最新 `llvm-config-N`（22 → 21 → 20 → 19 → 18），找不到再回退 `llvm-config`。
- CMake 最低版本仍是 LLVM 18，避免无必要地破坏旧环境。
- 修复 LLVM 22 对 `TargetRegistry::lookupTarget(StringRef)` 的弃用警告，改用 `Triple` 重载。
- 本地 `build/` 用 LLVM 22 重新配置并跑全量测试。

## IR 元数据决策

### 1. GEP inbounds

在 codegen 完成后遍历模块，将所有 `getelementptr` 标记为 `inbounds`。

理由：CDD 是 C 风格系统语言，合法程序的指针算术必须停留在同一分配对象内（允许 one-past-end）。越界指针算术本身是未定义行为。

### 2. 分配函数属性

对已知分配函数声明补充：

- `malloc`、`calloc`、`mi_malloc`、`mi_calloc`、`mi_malloc_aligned`：返回值 `noalias`，函数 `nounwind`
- `free`、`mi_free`：`nounwind`

`realloc`/`mi_realloc` **不加** `noalias` 返回值：它可能返回传入的同一指针。

### 3. TBAA

本轮不做。原因：

- `reinterpret_cast` 支持指针重解释，内存层类型 punning 的合法性未定义。
- 错误的 TBAA 可能让合法程序静默算错，风险高于收益。
- 如果后续要加，先写清“有效类型/严格别名”语言契约，并提供关闭开关。

## 验收

- LLVM 22 构建成功，全量 `ctest` 通过。
- 新增 driver 测试覆盖：
  - 指针算术 GEP 带 `inbounds`
  - `malloc` 声明带 `noalias`
- `bench_matmul`、`bench_sort`、`bench_hashmap`、`bench_nbody` 对比 LLVM 22 旧二进制无回退。
- 若元数据没有可测收益，回退 Task 2/3，只保留 LLVM 22 工具链升级。
