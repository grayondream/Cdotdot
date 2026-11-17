# CDD

[English](README.md) | **简体中文**

**CDD**（读作 “C dot dot”）是一门基于 LLVM 的静态类型、C 风格系统编程语言。
它保留 C 风格显式内存模型与 C 兼容 FFI，移除预处理器和旧式数值类型，并提供模块、
命名空间、类、单继承、模板、lambda、编译期求值、原子、线程，以及用 CDD 自身编写
的标准库。

 [![Language: CDD](https://img.shields.io/badge/language-CDD-1f6feb)](https://github.com/grayondream/Cdotdot) [![LLVM 18+](https://img.shields.io/badge/LLVM-18%2B-6f42c1)](https://llvm.org/) [![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)](https://isocpp.org/) [![Platform: Linux x86-64](https://img.shields.io/badge/platform-Linux%20x86--64-lightgrey)](#平台支持) [![Status: pre-1.0](https://img.shields.io/badge/status-pre--1.0-orange)](#状态与限制) [![Tests: 1900+](https://img.shields.io/badge/tests-1900%2B-brightgreen)](#测试)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> **项目状态：pre-1.0 / alpha。** 当前唯一支持的目标平台是 Linux x86-64。语言规范、
> ABI 与标准库 API 仍在稳定过程中。若需要可复现构建，请固定到某个 commit。参见
> [状态与限制](#状态与限制)与 [TODO.md](TODO.md)。

> 本文是 [README.md](README.md) 的简体中文版本。若中英文内容不一致，以英文版为准。

## 目录

- [项目概览](#项目概览)
- [快速开始](#快速开始)
- [使用方式](#使用方式)
- [语言导览](#语言导览)
- [标准库](#标准库)
- [仓库结构](#仓库结构)
- [编译架构](#编译架构)
- [测试](#测试)
- [文档](#文档)
- [状态与限制](#状态与限制)
- [贡献指南](#贡献指南)
- [许可证](#许可证)
- [致谢](#致谢)

## 项目概览

CDD 面向系统级代码：既需要 C 可预测的执行模型，又希望比文本 include 和宏更好的
组织结构。源文件使用 `.cdd` 后缀。标准库公开命名空间为 `std`；编译器内部 C++
命名空间为 `cdd`。

### 一览

| 属性 | 值 |
|---|---|
| 编程范式 | 命令式 / 系统级；过程式、面向对象、泛型 |
| 类型系统 | 静态、名义类型、定宽数值类型 |
| 内存模型 | 手动管理、C 风格；无垃圾回收 |
| 后端 | LLVM 18+ |
| 主要目标 | Linux x86-64 |
| 源文件后缀 | `.cdd` |
| 标准库命名空间 | `std` |
| 编译器实现 | C++20 |
| 状态 | Pre-1.0 / alpha |
| 许可证 | MIT；见[许可证](#许可证) |

### 设计原则

- **可预测的执行** — 定宽类型（`int8` 到 `int128`，`float16` 到 `float128`）、
  显式分配、无垃圾回收、无隐藏控制流。
- **显式依赖** — 用模块和 import 取代文本 include；用 `compile_time` /
  `compile_only` 取代宏。
- **面向系统** — 指针、切片、内联汇编、原子、线程，以及 C 兼容 FFI。
- **编译期元编程** — `compile_time` / `compile_only`、反射、布局查询，以及
  编译期容器求值。
- **CDD 原生标准库** — 标准库实现在 `libs/std/*.cdd`，而不是藏在编译器内部。

### CDD 不是什么

- 它目前**不是内存安全语言**：没有借用检查器，也没有垃圾回收；手动内存管理
  和未定义行为都可能出现。
- 它**不是 C++ 的平替**：类、继承和模板都是有意的子集；异常、RAII、虚函数和
  多继承不属于该语言。
- 它**尚未 ABI 稳定，也不跨平台**：当前支持 Linux x86-64。
- 它**不是 1.0 版本**：规范和实现仍在演进。

## 快速开始

### 环境要求

| 依赖 | 说明 |
|---|---|
| Linux x86-64 | 当前主要且唯一支持的目标 |
| CMake 3.20+ | 构建系统 |
| C++20 编译器 | GCC 或 Clang |
| LLVM 18+ | 已用 LLVM 22.1.8 验证 |
| `vcpkg`（可选） | 配置后提供 LLVM、`spdlog`、`cxxopts` |
| GoogleTest | 未安装时自动拉取 |
| `pthread` | 由驱动链接 |
| mimalloc | 内置于 `thirdparty/mimalloc` |

### 构建

```bash
git clone https://github.com/grayondream/Cdotdot.git
cd Cdotdot
./scripts/build.sh
```

Release 构建：

```bash
BUILD_TYPE=Release ./scripts/build.sh
```

或手动配置：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build build -j"$(nproc)"
```

如果 `llvm-config` 不在 `PATH` 中，请把 `LLVM_DIR` 设置为包含
`LLVMConfig.cmake` 的目录。如果使用 `vcpkg`，请在运行 `./scripts/build.sh`
之前设置 `VCPKG_ROOT`。

编译器输出到 `build/bin/cdd`。

### 安装（可选）

```bash
cmake --install build --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
```

编译器会与标准库一起安装到 `share/cdd/libs`。运行时标准库相对于编译器可执行
文件解析；也可以用 `CDD_STD_DIR` 覆盖搜索路径。

### Hello, CDD

创建 `hello.cdd`：

```cdd
module hello;

int32 main() {
    println("Hello, CDD!");
    return 0;
}
```

编译并运行：

```bash
build/bin/cdd hello.cdd -o hello
./hello
```

预期输出：

```text
Hello, CDD!
```

源文件的 `module` 声明必须与文件名一致：`module hello;` 对应 `hello.cdd`。

### 运行测试

```bash
ctest --test-dir build -j"$(nproc)" --output-on-failure
```

运行端到端功能巡览：

```bash
build/bin/cdd examples/all_features.cdd -o /tmp/all_features
/tmp/all_features
```

最后一行必须是 `ALL FEATURES OK`。

## 使用方式

```text
cdd [options] <input.cdd>
```

| 选项 | 说明 |
|---|---|
| `-c` | 只编译为目标文件，不链接 |
| `-o <file>` | 设置输出路径 |
| `-S` | 输出 LLVM IR，而不是可执行文件 |
| `-M <path>` | 添加模块导入搜索路径（可重复） |
| `-O <0-3>` | 设置优化级别 |
| `-g` | 生成调试信息 |
| `-v` | 打开驱动的详细输出 |
| `--no-prelude` | 不自动注入 `std.c` libc 绑定层 |
| `-Wall` | 打开所有警告 |
| `-Werror` | 将警告视为错误 |
| `-fsyntax-only` | 只做词法、语法和 import 检查；跳过语义分析与代码生成 |
| `-l <lib>` | 链接库（可重复） |
| `-L <path>` | 添加库搜索路径（可重复） |
| `-h`, `--help` | 显示命令行帮助 |

示例：

```bash
# 仅解析并检查 import
build/bin/cdd -fsyntax-only hello.cdd

# 输出 LLVM IR
build/bin/cdd -S hello.cdd -o hello.ll

# 编译为目标文件
build/bin/cdd -c hello.cdd -o hello.o

# 带优化的调试构建
build/bin/cdd hello.cdd -o hello -O 2 -g
```

运行 `build/bin/cdd --help` 查看完整选项。

### 模块

CDD 没有文本 include。一个文件只声明一个模块，import 使用点分模块路径：

```cdd
module app;
import std.vec;
import std.io;
```

导入会从当前导入文件所在目录、`-M` 提供的路径，以及已安装的标准库中解析。
`CDD_STD_DIR` 可以覆盖标准库根目录。除非传入 `--no-prelude`，否则会自动注入
`std.c` libc 绑定层。旧 `.smc` 源文件不受支持，会被拒绝并给出迁移提示。

### 标准库示例

```cdd
module vec_demo;
import std.vec;

int32 main() {
    std::Vec<int32> v = {};
    v.init();
    v.push(42);
    println("{}", v.get(0));
    v.destroy();
    return 0;
}
```

拥有所有权的容器和同步原语使用显式 `init` / `destroy` 生命周期；没有析构函数或
隐藏清理路径。

## 语言导览

下面是一个完整、可编译的 CDD 程序：

```cdd
module tour;

class Counter {
public:
    int32 count;
    void inc() { this->count = this->count + 1; }
};

template<typename T> struct Box { T value; };

compile_time int32 twice(int32 x) { return x * 2; }

int32 main() {
    Counter c;
    c.count = 0;
    c.inc();

    Box<int32> b;
    b.value = twice(c.count);
    println("{}", b.value);
    return 0;
}
```

输出 `2`。

其他已实现的语言能力包括：

- 定宽整数与浮点类型，包括 `int128`、`uint128`、`float16`、`float128`。
- 模块、命名空间、`struct` / `class` / `union` / `enum`、访问控制、单继承、
  位域和匿名类型。
- 类模板、函数模板、别名模板、非类型模板参数，以及 CRTP 风格的静态分派。
- Lambda 与闭包、函数重载、运算符重载、默认参数。
- 数组、切片（`T[]`）、`optional<T>`、`result<T, E>`、`defer`、指定初始化器、
  柔性数组成员和 C 风格字面量。
- 内联汇编、原子、线程、互斥量、条件变量，以及 C FFI。
- `compile_time` / `compile_only`、反射、布局查询、目标/构建信息，以及编译期
  容器求值。

完整功能巡览见 [`examples/all_features.cdd`](examples/all_features.cdd)。

## 标准库

标准库位于 `libs/std/`，通过点分模块名导入。公开名称通过 `std` 命名空间暴露。

| 领域 | 模块 |
|---|---|
| 核心与运行时 | `std.c`, `std.core`, `std.io`, `std.mem`, `std.bit`, `std.math` |
| 文本 | `std.string`, `std.ascii`, `std.parse`, `std.path` |
| 容器与算法 | `std.vec`, `std.deque`, `std.hashmap`, `std.hashset`, `std.hash`, `std.sort` |
| 系统与并发 | `std.fs`, `std.os`, `std.time`, `std.thread`, `std.atomic` |
| 测试 | `std.testing` |

标准库用 CDD 编写。容器和线程 API 使用显式资源生命周期；错误通过状态值或 panic
报告，而不是异常。

## 仓库结构

| 路径 | 内容 |
|---|---|
| `src/frontend/` | 词法器、token 流、解析器、运算符优先级 |
| `src/ast/` | AST 节点、类型、布局、名称修饰、编译期值 |
| `src/sema/` | 名称/类型解析、模板、编译期求值、反射、诊断 |
| `src/codegen/` | LLVM IR 生成 |
| `src/driver/` | CLI、模块加载、标准库预置层、链接 |
| `libs/std/` | 用 CDD 编写的标准库 |
| `examples/` | `all_features.cdd`，端到端功能巡览 |
| `tests/` | 单元、集成、端到端、驱动与规范一致性测试 |
| `benchmarks/` | 性能基线（CDD vs clang++/g++，运行时间、编译时间、代码大小） |
| `docs/spec/` | 规范性语言规范 |
| `docs/superpowers/` | 设计记录与历史实现计划 |
| `scripts/` | 构建脚本 |
| `thirdparty/mimalloc/` | 内置分配器 |
| `resources/` | 测试资源与历史 C 对比样例 |

## 编译架构

```text
.cdd source
    |
    v
Lexer -> Parser -> AST -> Semantic analysis -> LLVM IR -> object -> link
                        |
                        +-- compile_time evaluator / reflection
                        +-- layout and name mangling
```

- `CompilerDriver` 负责编排编译、模块预加载、标准库注入、优化和链接。
- `ModuleLoader` 解析 `.cdd` import；`StdPrelude` 注入 `std.c` libc 绑定层。
- `SemanticAnalyzer` 解析名称、类型、重载和模板；`CompileTimeEvaluator` 执行
  `compile_time` 代码。
- `CodegenContext` 把检查后的 AST 降低为 LLVM IR。`LayoutBuilder` 计算类型布局，
  `Mangle` 生成符号名。
- `Linker` 通过 `cc` 调用系统链接器，并链接 mimalloc、`pthread` 和用户请求的库。
- 测试套件使用 LLVM 的进程内 LLJIT 执行运行期测试。

编译器用 C++20 实现，内部 C++ 命名空间为 `cdd`。

## 测试

仓库包含单元、集成、驱动、代码生成和端到端测试，以及规范一致性测试。当前代码树有
1,900+ 个通过测试。

```bash
# 完整测试套件
ctest --test-dir build -j"$(nproc)" --output-on-failure

# 端到端功能巡览
build/bin/cdd examples/all_features.cdd -o /tmp/all_features
/tmp/all_features
```

功能巡览必须以状态 0 退出，并在最后一行打印 `ALL FEATURES OK`。

性能基线位于 [`benchmarks/`](benchmarks/README.md)。快速冒烟运行：

```bash
python3 benchmarks/run.py --quick
```

完整基线与方法学见 [`benchmarks/BASELINE.md`](benchmarks/BASELINE.md)。

目前仓库尚未配置自动化 CI；验证在本地执行。测试/CI 待办事项见 [TODO.md](TODO.md)。

## 文档

| 文档 | 内容 |
|---|---|
| [`README.md`](README.md) | English README |
| [`docs/spec/README.md`](docs/spec/README.md) | 规范索引与状态标记 |
| [`docs/spec/grammar.ebnf`](docs/spec/grammar.ebnf) | 文法与运算符优先级 |
| [`docs/spec/keywords.md`](docs/spec/keywords.md) | 关键字、保留字、运算符 |
| [`docs/spec/modules.md`](docs/spec/modules.md) | 模块、命名空间、导入可见性 |
| [`docs/spec/semantics.md`](docs/spec/semantics.md) | 求值、重载、静态检查 |
| [`docs/spec/conversions.md`](docs/spec/conversions.md) | 转换、提升、重载等级 |
| [`docs/spec/abi.md`](docs/spec/abi.md) | 类型布局、调用约定、名称修饰 |
| [`docs/spec/compile_time.md`](docs/spec/compile_time.md) | `compile_time` 语法、求值器、反射 |
| [`docs/spec/stdlib.md`](docs/spec/stdlib.md) | 内建与标准库参考 |
| [`docs/spec/fixed-width-types.md`](docs/spec/fixed-width-types.md) | 定宽数值类型模型 |
| [`TODO.md`](TODO.md) | 工程待办与路线图 |
| [`Progress.md`](Progress.md) | 按时间记录的实现进展 |

## 状态与限制

### 已实现

当前代码树包含定宽数值类型、模块与命名空间、带单继承的类、struct/union/enum
与位域、模板（类/函数/别名/非类型）、lambda 与闭包、运算符重载、切片、
`optional` / `result`、指定初始化器、柔性数组成员、内联汇编、C FFI、
`compile_time` / `compile_only`、反射、原子/线程/互斥量/条件变量，
以及 22 个标准库模块。

### 已知限制

- **平台：** 仅 Linux x86-64。其他 Linux 架构尚未测试；macOS 和 Windows 尚不
  支持。
- **ABI：** ABI 已有文档，但尚未冻结。聚合按值传参/返回、名称修饰和 C FFI
  边界情况仍在稳定中。
- **内存安全：** 手动内存管理，无 RAII、无析构函数、无异常、无垃圾回收。边界
  检查和整数溢出行为尚未完全规范。
- **语言表面：** 部分 C++ 风格语法扩展仍在与文法协调；模板是实用子集，不是完整
  的 C++ 模板元编程。
- **工具链：** 编译器目前是最小 CLI。包管理、LSP、格式化和文档生成目前是明确的
  非目标。
- **验证：** 尚未配置 CI 工作流；测试套件目前在本地运行。

### 非目标

CDD 有意排除：

- C 预处理器、文本 include 和宏；
- `goto` 与标签；
- 变长数组；
- 多继承、虚继承、虚函数、vtable 和 RTTI；
- 异常与栈展开；
- 垃圾回收与所有权/借用检查；
- 模板特化、SFINAE、可变参数模板、模板模板参数、concepts 和默认模板实参；
- 包管理、LSP、格式化和文档生成器。

`constexpr` 已从语言中移除。需要编译期可执行、运行期也可调用的代码请使用
`compile_time`；仅编译期存在的函数请使用 `compile_only`。

### 平台支持

| 平台 | 状态 |
|---|---|
| Linux x86-64 | 支持（主要目标） |
| 其他 Linux 架构 | 未测试 |
| macOS | 尚不支持 |
| Windows | 尚不支持 |

### 版本策略

CDD 处于 pre-1.0。稳定语言版本标志仍在规划中，尚未提供；规范与 ABI 可能随
commit 变化。需要可复现构建时，请固定到某个 commit 或 tag。

## 贡献指南

欢迎贡献，尤其是缺陷报告、测试用例、文档修复和范围明确的实现工作。

1. 开始较大的改动前，先阅读 [`TODO.md`](TODO.md) 和 [`Progress.md`](Progress.md)。
2. 较大的改动请先开 issue，讨论设计与范围。
3. 为新行为添加测试。项目在可行时遵循测试先行的工作流。
4. 修改解析器、语义或 ABI 时，同步更新 `docs/spec/`。
5. 提交 PR（pull request）前，运行完整测试套件和功能巡览。
6. 使用聚焦的提交信息，例如 `feat(AREA): ...`、`fix(AREA): ...` 或
   `docs: ...`。

缺陷报告请包含平台、LLVM 版本、`cdd -v` 输出，以及最小 `.cdd` 复现用例。

## 许可证

CDD 以 MIT 许可证发布。完整许可证文本见 [`LICENSE`](LICENSE)。

## 致谢

CDD 构建在 LLVM 之上，并使用 mimalloc 作为分配器后端。测试套件使用
GoogleTest；命令行驱动使用 cxxopts 和 spdlog。项目支持用 vcpkg 管理依赖。
详情见 [`thirdparty/`](thirdparty/) 与 [`vcpkg.json`](vcpkg.json)。
