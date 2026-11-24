# CDD 1.0 — ABI / 类型布局规范

> 对应 TODO：**TYP-27**、**MEM-14/15/16**、**MOD-15**、**CG-01/18**
> 状态：`[impl]` 以 `src/codegen/CodegenContext.cpp`、`src/ast/Mangle.cpp` 为准；`[plan]` 为目标，可能随 DEC 调整。
> 目标平台：默认跟随宿主 LLVM target（`module->getDataLayout()`）；交叉编译配置见 CG-16。

## 1. 标量类型映射与布局

| 源类型 | LLVM 类型 | 位宽 | 对齐 | 状态 | 备注 |
|---|---|---|---|---|---|
| `void` | `void` | — | — | `[impl]` | |
| `bool` | `i1` | 1 | 1B | `[impl]` | 存储按 1 字节 |
| `char` | `i8` | 8 | 1 | `[impl]` | |
| `int32` | `i32` | 32 | 4 | `[impl]` | 平台默认 |
| `int8/16/32/64/128` | `i8/i16/i32/i64/i128` | 8…128 | 1…16 | `[impl]` | |
| `uint8…uint128` | `i8…i128` | 同上 | 同上 | `[impl]` | 无符号仅为语义 |
| `char8/16/32` | `i8/i16/i32` | 8/16/32 | 1/2/4 | `[impl]` | TYP-05：uint8/16/32 别名，布局等同 |
| `isize` | `i64` | 64 | 8 | `[impl]` | **目标**：绑定目标指针宽度（TYP-03） |
| `usize` | `i64` | 64 | 8 | `[impl]` | 同上；Slice 长度也用 `i64` |
| `float32` | `float` | 32 | 4 | `[impl]` | |
| `float64` | `double` | 64 | 8 | `[impl]` | |
| `f16` | — | 16 | 2 | `[plan]` | TYP-04，含软件兜底 |
| `f128` | — | 128 | 16 | `[plan]` | TYP-04 |
| `enum` | 底层类型（默认 `i32`） | 见底层 | 见底层 | `[impl]` | 显式底层类型 `:uint8` 等（TYP-09/TYP-25） |
| `T*` | `ptr` (opaque) | 指针 | 指针 | `[impl]` | 多级 = 多 ptr |
| `T[N]` | `[N x T]` | N×size | align(T) | `[impl]` | |
| `T[]` (Slice) | `{ ptr, i64 }` | 16(64 位) | 8 | `[impl]` | **目标** `{ptr, usize}`，TYP-12 |
| `T?` (optional) | `{ i1, T }` | — | — | `[impl]` | `{ bool valid; T value; }`（TYP-13，2026-10-06 落地：判别标志在前） |
| `result<T,E>` | `{ i1, T, E }` | — | — | `[impl]` | `{ bool ok; T value; E error; }`（DEC-03 裁决，2026-10-06：加判别标志） |
| `string_view`（原 `str`） | `{ ptr, i64 }` | 16(64 位) | 8 | `[impl]` | 字节视图 `{ptr, len}`（P1-06）；零拷贝语义 |
| `string` | `{ ptr, i64, i64 }` | 24(64 位) | 8 | `[impl]` | 动态字符串 `{ptr, len, cap}`（P1-06；P1-09 补记 ABI 行，终审 M5） |
| `atomic T` | 同 `T` | 同 `T` | 同 `T` | `[impl]` | CON-01：限定符不改布局（仅整型标量）；访问经 LLVM atomic load/store/atomicrmw |
| `std::Atomic<T>` | `{ T }` | 同 `T` | 同 `T` | `[impl]` | STD-24：单字段封装；布局/对齐与 `T` 相同，无锁字段 |
| `FieldInfo` | `{ {ptr,i64}, i64, i64 }` | 32(64 位) | 8 | `[impl]` | CT-07 反射内建类型 |
| `Attribute` | `{ {ptr,i64} }` | 16(64 位) | 8 | `[impl]` | CT-07 反射内建类型 |

> `thread_local`（CON-04）：变量存储为 LLVM thread-local 全局（每线程副本）；布局/大小与普通全局一致。

> 原子 ABI（STD-24，2026-11-01）：`atomic T` 与 `std::Atomic<T>` 的 1/2/4/8 字节
> 访问使用同宽整数 carrier 和**自然对齐**（i64 为 align 8；此前模板/i64 字段曾
> 降到 align 4 并回退到外部 `__atomic_*`）。`bool` 使用 i8 carrier（LLVM 拒绝
> `atomic i1`），float/pointer/enum 经同宽 bitcast/ptrtoint。x86-64 上 1/2/4/8
> 字节为 lock-free；不支持 16 字节/`int128`/`float128`/聚合。`Thread` 以 `usize`
> 承载 `pthread_t`，字段布局为 `{usize, bool, bool, i32}`（v1 Linux x86-64）。

## 2. 聚合布局

### 2.1 struct `[impl]`
- 使用具名 `%struct.Name`；字段按声明顺序。
- 自然对齐：每个字段对齐到自身 ABI 对齐，结构体对齐到最大成员对齐，尾部按需填充（LLVM 默认规则）。
- 无隐藏字段、无 vtable、无填充字段语义。

### 2.2 class `[impl]`
- 布局等价 struct；成员函数不占空间（AGG-08/13）。
- **继承**：基类子对象作为派生类第一个字段（偏移 0）——已按 `classDecl->baseClass` 生成（INH-03）；多继承明确不支持（INH-02/NG-09）。
- 成员函数降级为自由函数 `Class_method(Class* self, ...)`（AGG-09，见 §4）。

### 2.3 union `[impl]`
- 大小为最大成员分配大小，对齐为最对齐成员对齐。
- 所有成员偏移 0；成员访问是对字段 0 的 GEP + bitcast。
- 不跟踪活跃成员（C 风格，AGG-17）。

### 2.4 enum `[impl]`
- 默认底层类型为 `int32`（`i32`）；可用 `enum E : uint8 { ... }` 显式指定任意整型底层类型，LLVM 层即该底层类型（TYP-09/TYP-25）。
- 枚举常量为编译期整数常量，参与常量折叠；固定大小由底层类型决定。
- 传参/返回按底层类型；窄于 `int32` 的枚举作可变参数时经 C 默认实参提升（见 `conversions.md` §9）。
- `enum ↔ int32` 不隐式转换的强制检查（TYP-09/20）仍为 `[plan]`。

### 2.5 位域 `[plan]`
- 尚未实现；布局对齐哪个 C ABI（System V / MSVC）与 `[[packed]]`/`[[align]]` 交互见 **DEC-08**（AGG-06 / CG-09）。

## 3. 传参 / 返回 ABI

| 场景 | 当前 `[impl]` | 目标 `[plan]` |
|---|---|---|
| 标量参数/返回 | 直接按 LLVM 标量 | C 约定（MEM-16） |
| 指针 | `ptr` | 同 C |
| struct/class/union 按值 | 作为 LLVM 聚合值传递 | 需明确 `byval`/`sret` 与寄存器分类（MEM-14） |
| Slice/optional/result | 作为 LLVM `{...}` 聚合值 | 冻结布局后再定 |
| 可变参数 | `isVarArg`（FUN-06） | 与 C varargs 兼容 |
| 调用约定 | LLVM 默认（宿主） | C 调用约定；可选其他（MEM-11/MEM-16） |

> 说明：当前实现依赖 LLVM 默认 lowering，**未显式标注** `byval`/`sret`/`signext`/`zeroext`。目标 ABI 必须在 MEM-14 冻结，并用 CG-18 ABI 一致性测试锁定。

## 4. 名称修饰（MOD-15）

当前规则（`src/ast/Mangle.cpp`）：

1. 经 `extern` 声明的**具体签名**标记为 C 名，**不修饰** → 直接 C ABI 链接；
   同名但非 extern 的 CDD 重载仍走修饰名（例如 extern `f(int32)` 保持
   `f`，CDD 重载 `f(int64)` 为 `cdd$f$f_int64`）。`main` 同样保持裸 `main`。
2. 限定名（namespace/class/module）用 `.` 连接：`ns::f` / `ns.f` → `ns.f`。
   `.` 不属于源标识符，因此不会与源程序中的 `ns_f` 冲突。
3. CDD 函数符号统一以 `cdd$f$` 开头；参与模块再插入模块限定：
   `cdd$fm<模块名长度>:<模块名>$`。函数名中的 `_` 双写，随后每个参数前加
   单个 `_` 和参数类型编码。例如 `f(int32)` → `cdd$f$f_int32`，零参数函数
   `f_int32()` → `cdd$f$f__int32`；模块 `a` 的 `f(int32)` →
   `cdd$fm1:a$f_int32`。可变参数函数在末尾追加 `_vararg`
   （`f(int32, ...)` → `cdd$f$f_int32_vararg`）。`$` 不属于源标识符，
   因此 CDD 函数与全局变量、C 符号及源码中形似修饰名的标识符互不冲突。
   参数编码是自定界的，函数名转义后单个 `_` 不会与类型名中的 `_` 混淆。
4. **static 成员（AGG-10，2026-10-05）**：类内 `static` 成员去糖为全局符号——
   `Class::member` → `Class.member`（与 namespace 限定名共用
   `mangleQualifiedName`）；随后按普通函数/全局变量规则叠加参数编码。static
   成员函数无 `this` 位。类在 namespace 内时，符号为
   `<ns前缀>.<类名>.<成员>_<参数编码>`（由 sema `scopedName` 统一附加），
   与访问侧全限定拼写 `ns::Class::member` 的编码结果严格一致。
5. **嵌套类型（AGG-11，2026-10-05）**：类/struct/union 体内声明的类型按
   `Outer::Inner` → `Outer.Inner` 限定键注册（与 namespace 限定类型共用
   `mangleQualifiedName`；namespace 组合 `ns::Outer::Inner` → `ns.Outer.Inner`）。
   嵌套类型的 static 成员/方法沿第 4 条规则以限定类名为路径
   （`ns::Outer::Inner::v` → `ns.Outer.Inner.v`）；类嵌套枚举常量注册于外层
   类作用域（`Outer::Red` → `Outer.Red`，不注册裸键）。
6. **文件作用域变量/数组**：`cdd$v$<名字>`；参与模块为
   `cdd$vm<模块名长度>:<模块名>$<名字>`。静态数据成员沿限定名规则编码。
   引用侧由 sema 记录 LLVM 符号，避免同名模块变量经扁平 codegen 作用域串线。
7. **编译器生成符号**：lambda 为 `cdd$lambda$...`、编译期容器表为
   `cdd$ct$...`、静态局部为 `cdd$static$...`；内部辅助函数使用带 `.` 的
   `cdd.` 前缀。它们都不含源码可拼写的普通标识符，避免与 C 名冲突。

`typeToMangled` 规则（标量名保持可读；所有变长名字用 `<长度>:<内容>` 自定界）：

| 类型 | 编码 | 类型 | 编码 |
|---|---|---|---|
| `void/int32/float32/float64/char/bool` | `void/int32/float32/float64/char/bool` | `T*` | `<T>ptr` |
| `int8..int128` | 同名 | `uint8..uint128` | 同名 |
| `isize/usize` | 同名 | `T&` | `<T>ref` |
| `A[N]` | `<A>arrs<N>`（含 `sizeParam` 时另加 `p<长度>:<参数名>`） | `T[]` | `<T>slice` |
| `T?` | `<T>opt` | `result<T,E>` | `<T>res<长度>:<E>` |
| struct/class/union | `N<长度>:<类型名>` | 命名 enum | `N<长度>:<枚举名>`；匿名 enum → `int32` |
| using 别名 | 展开到底层 | TypeVar | `X<长度>:<参数名>` |
| TypeInstance | `I<长度>:<模板名><类型实参个数>:...<值实参个数>:...<值参数名个数>:...` | 函数类型 | `<返回>fn<参数个数>:<参数...><v|n>` |

限定符在每一层参与编码：标量/名义类型用前缀 `C`/`V`/`atomic`；指针/引用、
数组、切片、optional、result 与函数类型把自身限定符放在构造器**后缀**，
避免与元素/返回类型的限定符前缀混淆（`slice<const int32>` ≠
`const slice<int32>`）。`using` 别名展开时把限定符合并到别名的顶层。
`restrict` 不参与编码。所有复合编码都可以作为另一个编码的组成部分而保持
单射，例如 `Box<A*>` 与 `Box<A>*`、`result<A,B>*` 与 `result<A,B*>`、
`A[N]` 与 `A[M]` 均生成不同符号。

**目标（MOD-15）**：名称必须稳定、可复现（INF-07），并与 C ABI / FFI 边界
清晰（MEM-12/ACC-05）。当前内部符号已采用上述单射编码；C 名保持裸名。

## 5. 端序与对齐

- 端序：跟随目标 `DataLayout`；规范不假定小端（端序规则见 MEM-15）。
- 默认对齐：LLVM 目标 ABI 对齐；`[[packed]]` / `[[align(N)]]` 尚未实现（ANN-03），交互见 DEC-08。
- 可复现构建要求输出路径无关、确定性（INF-07）。

## 6. 参考实现位置

- 类型映射：`CodegenContext::getLLVMType`（`src/codegen/CodegenContext.cpp:338`）
- 值转换：`CodegenContext::castValue`（:302）
- 名称修饰：`src/ast/Mangle.cpp`
- 类型定义：`src/ast/Type.h`
