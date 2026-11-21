# LLVM 22 与 IR 元数据优化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把本地 CDD 构建切到 LLVM 22，并用低风险 GEP/分配函数元数据验证生成代码是否还能变快。

**Architecture:** 先升级工具链并跑全量测试，形成干净的 LLVM 22 基线；再在 codegen 结束时集中补充 IR 元数据，最后用现有 runner 做 A/B 和全量验证。TBAA 因别名语义未定义而不在本轮实现。

**Tech Stack:** C++20、LLVM 22.1.2、CMake、GoogleTest、现有 Python benchmark runner。

**Spec:** `docs/superpowers/specs/2026-11-21-llvm22-ir-metadata-design.md`

## Global Constraints

- 基准方法保持：`-O2`、clang++-22、g++-15、5 warmup + 9 measured、矩阵 2+3。
- 默认 CPU 仍为 `generic`（SSE2）；`-march=native` 保持可选。
- CMake 最低 LLVM 版本仍为 18。
- 不使用 TBAA、fast-math、PGO、BOLT。
- 每个代码任务先写失败测试，再实现，再跑目标测试和全量测试。

## Review Focus

- 指针算术越界时 `inbounds` 会扩大 UB：确认合法程序与测试不依赖越界 GEP。
- `realloc` 可能返回入参指针：绝不能给 `realloc` 返回值加 `noalias`。
- 外部函数可能由用户或 FFI 重新声明：只给已知 libc/mimalloc 名称加属性。
- LLVM 22 的 `lookupTarget` 重载在 LLVM 18 不存在：保留旧路径的版本保护。
- 元数据无收益时不要用“IR 更规范”作为保留理由：按验收标准回退。

---

### Task 1: 切换到 LLVM 22 工具链

**Files:**
- Modify: `src/driver/CompilerDriver.cpp:75`
- Modify: `scripts/build.sh:20-36`
- Modify: `README.md:96-99`
- Modify: `README_zh.md` 对应环境表

**Interfaces:**
- Produces: 本地 `build/` 使用 LLVM 22；`./scripts/build.sh` 在存在 `llvm-config-22` 时优先选 22。

- [ ] **Step 1: 修复 `lookupTarget` 弃用调用**

在 `CompilerDriver.cpp` 中用版本保护选择 `Triple` 重载：

```cpp
#if LLVM_VERSION_MAJOR >= 19
    const llvm::Target* target =
        llvm::TargetRegistry::lookupTarget(llvmTriple, error);
#else
    const llvm::Target* target =
        llvm::TargetRegistry::lookupTarget(llvmTriple.str(), error);
#endif
```

- [ ] **Step 2: 构建脚本优先最新 llvm-config**

将 `scripts/build.sh` 的探测顺序改为：显式 `LLVM_DIR` → `llvm-config-22` → `llvm-config-21` → `llvm-config-20` → `llvm-config-19` → `llvm-config-18` → `llvm-config`。每个候选都用 `--cmakedir`。

- [ ] **Step 3: 更新文档**

README 环境表写“Tested with LLVM 22.1.2”；构建说明说明脚本会优先最新的 `llvm-config-N`，也可显式覆盖 `LLVM_DIR`。

- [ ] **Step 4: 用 LLVM 22 重新配置 build**

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm \
  -Dfmt_DIR=/usr/lib/x86_64-linux-gnu/cmake/fmt \
  -Dspdlog_DIR=/usr/lib/x86_64-linux-gnu/cmake/spdlog
cmake --build build -j"$(nproc)"
```

预期：`Found LLVM 22.1.2`，无 `lookupTarget` 弃用警告，`build/bin/cdd` 链接 `libLLVM.so.22.1`。

- [ ] **Step 5: 全量测试**

```bash
ctest --test-dir build -j8 --output-on-failure
```

预期：`1935/1935` 通过。

- [ ] **Step 6: 提交**

```bash
git add src/driver/CompilerDriver.cpp scripts/build.sh README.md README_zh.md
git commit -m "build: 切换到 LLVM 22 工具链"
```

---

### Task 2: 给 GEP 补充 inbounds

**Files:**
- Modify: `src/codegen/CodegenContext.h`
- Modify: `src/codegen/CodegenContext.cpp`
- Modify: `src/driver/CompilerDriver.cpp`
- Test: `tests/driver/test_compiler_driver.cpp`

**Interfaces:**
- Produces: `void CodegenContext::applyOptimizationMetadata();`
- Driver 在 `finalizeDebugInfo()` 之后、优化之前调用。

- [ ] **Step 1: 写失败测试**

在 `tests/driver/test_compiler_driver.cpp` 增加 `IRMarksPointerArithmeticInBounds`。源文件用 `int32* p` 和 `*(p + i)`，以 `-S -O 0` 运行，断言：

```cpp
EXPECT_NE(ir.find("getelementptr inbounds i32, ptr"), std::string::npos);
EXPECT_EQ(ir.find("getelementptr i32, ptr"), std::string::npos);
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cmake --build build -j"$(nproc)" --target compiler_tests
./build/bin/compiler_tests --gtest_filter='CompilerDriverTest.IRMarksPointerArithmeticInBounds'
```

预期：找到非 inbounds 的 `getelementptr i32`，测试失败。

- [ ] **Step 3: 实现 `applyOptimizationMetadata`**

在 `CodegenContext.cpp` 中遍历模块内所有指令，对 `GetElementPtrInst` 调用 `setIsInBounds(true)`。在 driver 的 `codegenCtx.finalizeDebugInfo();` 后调用。

- [ ] **Step 4: 运行目标测试**

```bash
cmake --build build -j"$(nproc)" --target compiler_tests
./build/bin/compiler_tests --gtest_filter='CompilerDriverTest.IRMarksPointerArithmeticInBounds'
```

预期：PASS。

- [ ] **Step 5: 提交**

```bash
git add src/codegen/CodegenContext.h src/codegen/CodegenContext.cpp src/driver/CompilerDriver.cpp tests/driver/test_compiler_driver.cpp
git commit -m "perf(codegen): 标记 GEP 为 inbounds"
```

---

### Task 3: 给分配函数补充 noalias/nounwind

**Files:**
- Modify: `src/codegen/CodegenContext.cpp` (`applyOptimizationMetadata`)
- Test: `tests/driver/test_compiler_driver.cpp`

**Interfaces:**
- Consumes: `CodegenContext::applyOptimizationMetadata()` from Task 2.
- Produces: `malloc`/`calloc`/`mi_malloc`/`mi_calloc`/`mi_malloc_aligned` 返回 `noalias` + `nounwind`；`free`/`mi_free` `nounwind`。

- [ ] **Step 1: 写失败测试**

增加 `IRMarksMallocNoAlias`：源程序调用 `malloc` 和 `free`，以 `-S -O 0` 运行，断言：

```cpp
EXPECT_NE(ir.find("declare noalias ptr @malloc"), std::string::npos);
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cmake --build build -j"$(nproc)" --target compiler_tests
./build/bin/compiler_tests --gtest_filter='CompilerDriverTest.IRMarksMallocNoAlias'
```

预期：当前输出是 `declare ptr @malloc`，测试失败。

- [ ] **Step 3: 实现已知分配函数属性**

在 `applyOptimizationMetadata` 中遍历函数声明：

- `malloc`、`calloc`、`mi_malloc`、`mi_calloc`、`mi_malloc_aligned`：`addRetAttr(NoAlias)`、`addFnAttr(NoUnwind)`
- `free`、`mi_free`：`addFnAttr(NoUnwind)`
- 不给 `realloc`/`mi_realloc` 加返回 `noalias`

- [ ] **Step 4: 运行目标测试**

```bash
cmake --build build -j"$(nproc)" --target compiler_tests
./build/bin/compiler_tests --gtest_filter='CompilerDriverTest.IRMarksMallocNoAlias'
```

预期：PASS。

- [ ] **Step 5: 提交**

```bash
git add src/codegen/CodegenContext.cpp tests/driver/test_compiler_driver.cpp
git commit -m "perf(codegen): 标记分配函数 noalias/nounwind"
```

---

### Task 4: 验证元数据收益与全量状态

**Files:**
- Modify: `TODO.md`
- Modify: `Progress.md`
- If retained: regenerate `benchmarks/BASELINE.md` and `benchmarks/results/2026-11-21-*.{json,md}`

**Interfaces:**
- Consumes: LLVM 22 build + metadata changes.
- Produces: 有数据支撑的“保留/回退”决策。

- [ ] **Step 1: 全量测试**

```bash
ctest --test-dir build -j8 --output-on-failure
```

预期：`1937/1937`（新增 2 个 driver 测试）。

- [ ] **Step 2: 直接 A/B 对比元数据前后**

用同一 LLVM 22 构建，交替运行 `bench_matmul`、`bench_sort`、`bench_hashmap`、`bench_nbody`，每项至少 5 warmup + 12 measured，取中位数。

- [ ] **Step 3: 决策**

- 若任一核心负载回退 >2% 或总体 core geomean 变差：回退 Task 2/3 的元数据提交，保留 LLVM 22。
- 若有可测收益或严格持平：保留，并用 `--full --baseline-md` 重跑最终基线。

- [ ] **Step 4: 更新 TODO/Progress/提交**

记录 LLVM 22 版本、元数据决策、测试数、benchmark 数字和回退原因（如有）。

```bash
git add TODO.md Progress.md benchmarks/BASELINE.md benchmarks/results
git commit -m "docs: 记录 LLVM 22 与 IR 元数据验证结果"
```
