# Qualifier Type Core and Const Semantics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make CDD's `const` semantics C-correct by introducing a safe qualifier-aware type layer, qualifier-aware pointer compatibility, const lvalue propagation, const member functions, and const-aware built-in methods.

**Architecture:** Add a small `TypeUtils` layer that owns type cloning, qualifier application, type identity, and qualified printing. The parser stops mutating shared `Type` objects. Sema uses explicit qualifier conversion rules for initialization/assignment/arguments/returns, propagates object constness into member lvalues, and checks const member/built-in calls. Codegen only needs to respect the resulting lvalue constness for aggregate writes and globals.

**Tech Stack:** C++20, LLVM 22, GoogleTest, CMake, existing Lexer/Parser/SemanticAnalyzer/CodegenContext.

**Spec:** `docs/superpowers/specs/2026-11-23-qualifiers-references-design.md`

## Global Constraints

- Follow C qualifier semantics for `const`; `volatile`/`restrict` are implemented in the next plan, but the type layer here must preserve all three flags.
- Never mutate a shared `Type` object. Every qualifier application clones the complete type, including aggregate subclasses.
- Type identity includes `const`/`volatile` at every level. `restrict` is optimization-only and is not part of identity, overload matching, or mangling.
- Function signatures use C adjustment: top-level cv on by-value parameters/returns is ignored; pointee cv is part of identity and mangling.
- A const object's member is a const lvalue. A pointer member of a const object is a const pointer, not a pointer to const.
- Whole-aggregate assignment is rejected if the destination contains a const subobject; copy-initialization from such an aggregate remains legal.
- No new CLI flags. This is a correctness change.
- Full `compiler_tests` suite and `examples/all_features.cdd` must stay green.
- Use LLVM 22 (`clang++-22`, `opt-22`, `llc-22`); do not downgrade.
- Work directly in the current repository on `master`; no worktree (user constraint).

## Review Focus

- Multi-level pointer conversion: `T** -> const T**` must be rejected; `T** -> T* const*` must be accepted.
- Const object with a pointer member: `const S { int32* p; }` must expose `int32* const p`, not `const int32* p`.
- Const aggregate assignment: a destination with a const field/array element/base must not be written as a whole, while copy-initialization must still work.
- Pointer spelling: diagnostics and mangling must distinguish `const int32*`, `int32* const`, and `int32* const restrict`.
- Built-in const dispatch: `const string` must reject `append`/`push`/`destroy`, and `data()` must yield `const char*`.

---

### Task 1: Qualifier-aware type core

**Files:**
- Create: `src/ast/TypeUtils.h`
- Create: `src/ast/TypeUtils.cpp`
- Modify: `src/frontend/Parser.cpp` (`parseType`, around lines 3055-3210)
- Modify: `src/CMakeLists.txt` only if the source glob needs a reconfigure note
- Test: `tests/sema/test_qualifiers.cpp` (create)
- Test: `tests/frontend/test_parser.cpp`

**Interfaces:**
- Consumes: `Type`, `TypeKind`, `StructType`, `ClassType`, `UnionType`, `EnumType`, `TypedefType`, `ArrayType`, `SliceType`, `OptionalType`, `ResultType`, `FunctionType`.
- Produces:

```cpp
struct TypeQualifiers {
    bool isConst = false;
    bool isVolatile = false;
    bool isRestrict = false;
};

Type* cloneType(const Type* type);
Type* qualifyType(const Type* type, TypeQualifiers qualifiers);
Type* qualifyTopLevel(const Type* type, bool isConst, bool isVolatile);
bool typesIdentical(const Type* a, const Type* b);
std::string qualifiedTypeToString(const Type* type);
```

- `cloneType` returns a new type with the same kind/subclass payload and flags.
- `qualifyType` ORs the given qualifiers onto the cloned top level; it does not recurse into pointer pointees.
- `qualifyTopLevel` is the parser-facing helper for `const`/`volatile` applied to a declared base type.
- `typesIdentical` compares every qualifier level, reference-ness, pointer targets, aggregate names, array lengths, and function signatures. It does not compare `isRestrict`.
- `qualifiedTypeToString` renders `const int32*`, `int32* const`, `const int32* const restrict` correctly.

- [ ] **Step 1: Write failing type-core tests**

Create `tests/sema/test_qualifiers.cpp` with this analysis helper and the first tests:

```cpp
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "ast/Decl.h"
#include "ast/Type.h"
#include "ast/TypeUtils.h"
#include "frontend/Lexer.h"
#include "frontend/Parser.h"
#include "sema/SemanticAnalyzer.h"

namespace {

struct AnalyzeResult {
    std::vector<std::string> errors;
    bool parseFailed = false;
};

AnalyzeResult analyzeSource(const std::string& source) {
    AnalyzeResult result;
    Lexer lexer("qualifiers.cdd", source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) {
        result.parseFailed = true;
        return result;
    }
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    for (const auto& error : analyzer.getErrors())
        result.errors.push_back(error.message);
    return result;
}

bool hasError(const AnalyzeResult& result, const std::string& needle) {
    for (const auto& error : result.errors)
        if (error.find(needle) != std::string::npos) return true;
    return false;
}

Type* parseTypeOnly(const std::string& typeSpelling) {
    Lexer lexer("type.cdd", typeSpelling + " v;");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return nullptr;
    auto* var = dynamic_cast<VarDeclAST*>(ast->declarations.front().get());
    return var ? var->type : nullptr;
}

std::unique_ptr<TranslationUnitAST> parseModule(const std::string& source) {
    Lexer lexer("module.cdd", source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return nullptr;
    return ast;
}

} // namespace

TEST(QualifierTypeCore, ParsesPointeeAndPointerQualifiersSeparately) {
    Type* pointeeConst = parseTypeOnly("const int32*");
    ASSERT_NE(pointeeConst, nullptr);
    EXPECT_FALSE(pointeeConst->isConst);
    ASSERT_NE(pointeeConst->base, nullptr);
    EXPECT_TRUE(pointeeConst->base->isConst);

    Type* pointerConst = parseTypeOnly("int32* const");
    ASSERT_NE(pointerConst, nullptr);
    EXPECT_TRUE(pointerConst->isConst);
    ASSERT_NE(pointerConst->base, nullptr);
    EXPECT_FALSE(pointerConst->base->isConst);
}

TEST(QualifierTypeCore, PrintsQualifiersAtTheirOwnLevel) {
    EXPECT_EQ(qualifiedTypeToString(parseTypeOnly("const int32*")),
              "const int32*");
    EXPECT_EQ(qualifiedTypeToString(parseTypeOnly("int32* const")),
              "int32* const");
    EXPECT_EQ(qualifiedTypeToString(parseTypeOnly("int32* restrict")),
              "int32* restrict");
    EXPECT_EQ(qualifiedTypeToString(parseTypeOnly("const int32* const restrict")),
              "const int32* const restrict");
}

TEST(QualifierTypeCore, DoesNotMutateSharedAggregateTypes) {
    auto module = parseModule(R"(
        struct S { int32 x; };
        const S* p;
        S q;
    )");
    ASSERT_NE(module, nullptr);
    auto* q = dynamic_cast<VarDeclAST*>(module->declarations[2].get());
    ASSERT_NE(q, nullptr);
    EXPECT_FALSE(q->type->isConst);
}

TEST(QualifierTypeCore, IdentityDistinguishesPointeeConst) {
    EXPECT_FALSE(typesIdentical(parseTypeOnly("int32*"),
                                parseTypeOnly("const int32*")));
    EXPECT_FALSE(typesIdentical(parseTypeOnly("int32*"),
                                parseTypeOnly("int32* const")));
    EXPECT_TRUE(typesIdentical(parseTypeOnly("const int32*"),
                               parseTypeOnly("const int32*")));
}
```

`parseTypeOnlyToModule` is a local helper that parses a full translation unit
and returns the AST, so the shared-aggregate test can inspect the second
declaration.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake -S . -B build && cmake --build build -j12 --target compiler_tests && ./build/bin/compiler_tests --gtest_filter='QualifierTypeCore.*'`

Expected: FAIL to compile because `ast/TypeUtils.h` and the new helpers do not
exist, or the parser tests report the current mutation.

- [ ] **Step 3: Implement `TypeUtils` and stop parser mutation**

Add `TypeUtils.h/.cpp` with the exact interface above. Implement `cloneType`
with a switch over all `TypeKind` values, cloning subclass payloads and
preserving flags/annotations. Implement `qualifyType` and
`qualifyTopLevel` via `cloneType`.

Replace the scalar-only clone plus unconditional writes in `Parser::parseType`
with `qualifyTopLevel`. The parser must never write `baseType->isConst`,
`baseType->isVolatile`, or `baseType->isRestrict` directly on a shared type.

- [ ] **Step 4: Run the focused tests**

Run: `cmake --build build -j12 --target compiler_tests && ./build/bin/compiler_tests --gtest_filter='QualifierTypeCore.*'`

Expected: PASS.

- [ ] **Step 5: Run the frontend/parser tests**

Run: `./build/bin/compiler_tests --gtest_filter='ParserTest.*:LexerTest.*'`

Expected: PASS. Fix any test that depended on shared-type mutation.

- [ ] **Step 6: Commit**

```bash
git add src/ast/TypeUtils.h src/ast/TypeUtils.cpp src/frontend/Parser.cpp tests/sema/test_qualifiers.cpp tests/frontend/test_parser.cpp
git commit -m "refactor: add qualifier-aware type core"
```

---

### Task 2: Signature identity and mangling

**Files:**
- Modify: `src/ast/Symbol.cpp` (`typesEqual` call sites for overloads, if needed)
- Modify: `src/ast/Symbol.h`
- Modify: `src/ast/Mangle.cpp`
- Modify: `src/sema/SemanticAnalyzer.cpp` (function signature construction)
- Test: `tests/sema/test_qualifiers.cpp`

**Interfaces:**
- Consumes: `typesIdentical`, `cloneType`, `qualifyType`.
- Produces:

```cpp
Type* adjustParameterTypeForSignature(const Type* type);
std::string typeToMangledSignature(const Type* type);
```

- `adjustParameterTypeForSignature` strips top-level `const`/`volatile`/
  `restrict` from by-value parameters and returns; it does not strip pointee
  qualifiers.
- `typeToMangledSignature` encodes pointee cv and reference-ness; it never
  encodes `restrict` or top-level cv.
- Function overload keys and duplicate-signature checks use
  `typesIdentical(adjustedA, adjustedB)`.

- [ ] **Step 1: Write failing signature tests**

Add:

```cpp
TEST(QualifierSignature, TopLevelConstByValueIsNotAnOverload) {
    EXPECT_EQ(typeToMangledSignature(parseTypeOnly("const int32")),
              typeToMangledSignature(parseTypeOnly("int32")));
    EXPECT_TRUE(typesIdentical(
        adjustParameterTypeForSignature(parseTypeOnly("const int32")),
        adjustParameterTypeForSignature(parseTypeOnly("int32"))));
}

TEST(QualifierSignature, PointeeConstIsAnOverload) {
    EXPECT_NE(typeToMangledSignature(parseTypeOnly("int32*")),
              typeToMangledSignature(parseTypeOnly("const int32*")));
    EXPECT_FALSE(typesIdentical(
        adjustParameterTypeForSignature(parseTypeOnly("int32*")),
        adjustParameterTypeForSignature(parseTypeOnly("const int32*"))));
}

TEST(QualifierSignature, RestrictIsNotAnOverload) {
    EXPECT_EQ(typeToMangledSignature(parseTypeOnly("int32* restrict")),
              typeToMangledSignature(parseTypeOnly("int32*")));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierSignature.*'`

Expected: FAIL because the helpers/mangling rules do not exist.

- [ ] **Step 3: Implement signature adjustment and mangling**

Update `typeToMangled` or add `typeToMangledSignature` to encode `const` and
`volatile` at pointee levels and to use a distinct reference marker. Strip
top-level qualifiers and restrict before signature identity. Update overload
registration/lookup and duplicate-signature diagnostics to use the adjusted
identity.

- [ ] **Step 4: Run focused tests**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierSignature.*:SemanticAnalyzerTest.*Overload*'`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/ast/Symbol.h src/ast/Symbol.cpp src/ast/Mangle.cpp src/sema/SemanticAnalyzer.cpp tests/sema/test_qualifiers.cpp
git commit -m "feat: add qualifier-aware signatures and mangling"
```

---

### Task 3: C pointer compatibility and assignment

**Files:**
- Modify: `src/sema/SemanticAnalyzer.cpp` (`typesCompatible`, `checkAssignmentTypes`)
- Modify: `src/ast/Symbol.cpp` (`conversionRank`)
- Test: `tests/sema/test_qualifiers.cpp`

**Interfaces:**
- Consumes: `typesIdentical`, qualifier helpers.
- Produces:

```cpp
bool pointerConversionAllowed(const Type* target, const Type* source);
int qualifiedConversionRank(const Type* from, const Type* to);
```

- `pointerConversionAllowed` implements C qualifier conversion:
  - base types must match (or permitted `void*`/derived-to-base cases);
  - adding `const`/`volatile` at a pointee level is allowed;
  - dropping is rejected;
  - multi-level unsafe conversions are rejected;
  - `restrict` is ignored.
- `qualifiedConversionRank` returns `0` exact, `1` qualifier addition,
  `2` allowed pointer/void conversion, `-1` invalid.
- Integer-to-pointer is allowed only for a null pointer constant `0`.

- [ ] **Step 1: Write failing pointer-compatibility tests**

Add positive and negative tests:

```cpp
TEST(QualifierPointers, AcceptsQualifierAddition) {
    EXPECT_TRUE(analyzeSource(R"(
        int32 main() {
            int32 x = 1;
            int32* p = &x;
            const int32* cp = p;
            volatile int32* vp = p;
            int32** pp = &p;
            int32* const* cpp = pp;
            return *cp + *vp + **cpp;
        }
    )").errors.empty());
}

TEST(QualifierPointers, RejectsQualifierDrop) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        int32 main() {
            int32 x = 1;
            const int32* p = &x;
            int32* q = p;
            return *q;
        }
    )"), "const"));
}

TEST(QualifierPointers, RejectsUnsafeMultiLevelAddition) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        int32 main() {
            int32 x = 1;
            int32* p = &x;
            int32** pp = &p;
            const int32** cpp = pp;
            return 0;
        }
    )"), "const"));
}

TEST(QualifierPointers, RejectsDifferentPointeeTypes) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        int32 main() {
            float64 f = 1.0;
            float64* fp = &f;
            int32* ip = fp;
            return 0;
        }
    )"), "float64"));
}

TEST(QualifierPointers, RejectsArbitraryIntegerToPointer) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        int32 main() {
            int32 x = 5;
            int32* p = x;
            return 0;
        }
    )"), "pointer"));
}

TEST(QualifierPointers, AcceptsNullPointerConstant) {
    EXPECT_TRUE(analyzeSource("int32 main() { int32* p = 0; return p == null ? 0 : 1; }").errors.empty());
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierPointers.*'`

Expected: FAIL on the const-drop, unsafe multi-level, and different-pointee
cases.

- [ ] **Step 3: Implement pointer conversion rules**

Replace the blanket pointer shortcuts in `typesCompatible`,
`checkAssignmentTypes`, and `conversionRank` with
`pointerConversionAllowed`/`qualifiedConversionRank`. Keep arithmetic
conversions independent of top-level qualifiers. Strip typedefs before
qualifier decisions. Check null pointer constants at the expression/codegen
boundary rather than accepting every integer.

- [ ] **Step 4: Run focused tests and existing pointer tests**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierPointers.*:PointerCompareE2E.*:PointerArithmetic*'`

Expected: PASS. Update existing tests only where they asserted the old
permissive behavior.

- [ ] **Step 5: Commit**

```bash
git add src/sema/SemanticAnalyzer.cpp src/ast/Symbol.cpp tests/sema/test_qualifiers.cpp
git commit -m "fix: enforce C pointer qualifier conversions"
```

---

### Task 4: Const lvalue propagation and aggregate assignment

**Files:**
- Modify: `src/sema/SemanticAnalyzer.cpp` (`visit(MemberAccessExprAST)`, `visit(ArrayAccessExprAST)`, `visit(UnaryExprAST)`, `visit(AssignmentExprAST)`)
- Modify: `src/sema/SemanticAnalyzer.h`
- Modify: `src/ast/TypeUtils.h/.cpp`
- Test: `tests/sema/test_qualifiers.cpp`
- Test: `tests/e2e/test_qualifiers.cpp` (create)

**Interfaces:**
- Consumes: `qualifyType`, `qualifyTopLevel`, `typesIdentical`.
- Produces:

```cpp
Type* qualifyMemberLValue(const Type* memberType, bool objectIsConst,
                          bool objectIsVolatile);
bool isModifiableLValueType(const Type* type, std::string* reason);
```

- `qualifyMemberLValue` adds top-level qualifiers to a member; for array
  members it qualifies the element type; it does not qualify pointer pointees
  or reference targets.
- `isModifiableLValueType` recursively rejects const scalar/aggregate/array/
  base subobjects and (later) reference-containing aggregates.

- [ ] **Step 1: Write failing const-lvalue tests**

```cpp
TEST(QualifierConstLValue, RejectsMemberWriteThroughConstObject) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct S { int32 x; };
        int32 main() { const S s = {1}; s.x = 2; return s.x; }
    )"), "const"));
}

TEST(QualifierConstLValue, RejectsMemberWriteThroughConstPointer) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct S { int32 x; };
        int32 main() { S s = {1}; const S* p = &s; p->x = 2; return s.x; }
    )"), "const"));
}

TEST(QualifierConstLValue, ConstObjectPointerMemberIsConstPointer) {
    auto result = analyzeSource(R"(
        struct S { int32* p; };
        int32 main() {
            int32 x = 1;
            const S s = {&x};
            *s.p = 2;
            return x;
        }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(QualifierConstLValue, RejectsAddressOfConstObjectToMutablePointer) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct S { int32 x; };
        int32 main() { const S s = {1}; S* p = &s; return 0; }
    )"), "const"));
}

TEST(QualifierConstLValue, RejectsWholeConstAggregateAssignment) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct S { int32 x; };
        int32 main() { const S a = {1}; S b = {2}; a = b; return 0; }
    )"), "const"));
}

TEST(QualifierConstLValue, AllowsCopyInitializationFromConstAggregate) {
    auto result = analyzeSource(R"(
        struct S { int32 x; };
        int32 main() { const S a = {1}; S b = a; b.x = 2; return b.x; }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(QualifierConstLValue, RejectsAggregateWithConstFieldAssignment) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct S { const int32 x; };
        int32 main() { S a = {1}; S b = {2}; a = b; return 0; }
    )"), "const"));
}

TEST(QualifierConstLValue, ConstAliasPreservesReadonly) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        using CI = const int32;
        int32 main() { CI x = 1; x = 2; return x; }
    )"), "const"));
}
```

Add an e2e test that `const S` with a pointer member can still write the
pointee:

```cpp
TEST_F(QualifiersE2E, ConstObjectPointerMemberKeepsMutablePointee) {
    EXPECT_EQ(runQualifierSource(R"(
        struct S { int32* p; };
        int32 main() {
            int32 x = 1;
            const S s = {&x};
            *s.p = 42;
            return x - 42;
        }
    )"), 0);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierConstLValue.*:QualifiersE2E.ConstObjectPointerMemberKeepsMutablePointee'`

Expected: FAIL on the accepted non-modifiable writes and on the diagnostic
text.

- [ ] **Step 3: Implement lvalue qualification and modifiable checks**

Add `qualifyMemberLValue` and use it in member/array/deref lvalue typing.
Update `visit(AssignmentExprAST)` to check `isModifiableLValueType` on the
stripped lvalue type. For aggregates, reject a whole-object store if any
field/array element/base subobject is const. Keep copy-initialization on the
source side allowed. Ensure codegen never emits a whole-aggregate store into
a destination containing a const subobject.

- [ ] **Step 4: Run focused and full sema tests**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierConstLValue.*:QualifiersE2E.*:SemanticAnalyzerTest.*Const*'`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/sema/SemanticAnalyzer.cpp src/sema/SemanticAnalyzer.h src/ast/TypeUtils.h src/ast/TypeUtils.cpp tests/sema/test_qualifiers.cpp tests/e2e/test_qualifiers.cpp
git commit -m "fix: propagate const through lvalues and aggregates"
```

---

### Task 5: Const member functions and const `this`

**Files:**
- Modify: `src/frontend/Parser.cpp` (function/method declaration parsing)
- Modify: `src/ast/Type.h` (`FunctionType` metadata) or `src/ast/Decl.h` (`FunctionDeclAST`)
- Modify: `src/sema/SemanticAnalyzer.cpp` (`visit(MethodDecl)`, `visit(MemberAccessExprAST)`, `visit(MethodCallExprAST)`, `this` typing)
- Test: `tests/frontend/test_parser.cpp`
- Test: `tests/sema/test_qualifiers.cpp`
- Test: `tests/e2e/test_qualifiers.cpp`

**Interfaces:**
- Consumes: qualifier core and lvalue propagation.
- Produces:

```cpp
class FunctionDeclAST {
public:
    bool isConstMethod = false;
};

class FunctionType : public Type {
public:
    bool isConstMethod = false;
};
```

- The parser sets `FunctionDeclAST::isConstMethod`; declaration registration
  copies it into the `FunctionType` used by method lookup.
- A const method's implicit `this` type is `const Class*`; a non-const method's
  is `Class*`.
- Const object method calls reject non-const methods.
- Const methods reject field writes and non-const method calls on `this`.
- `static` methods cannot be `const`.
- Const-only overloads are diagnosed as duplicates.

- [ ] **Step 1: Write failing parser/sema/e2e tests**

Parser:

```cpp
TEST(ParserTest, ParsesConstMemberFunction) {
    auto tu = parse("class C { public: int32 get() const { return 1; } };");
    ASSERT_NE(tu, nullptr);
    EXPECT_TRUE(parser.getErrors().empty());
}
```

Sema:

```cpp
TEST(QualifierConstMethods, ConstObjectCanCallConstMethod) {
    EXPECT_TRUE(analyzeSource(R"(
        class C { public: int32 v; int32 get() const { return this->v; } };
        int32 main() { C c; c.v = 1; const C* p = &c; return p->get(); }
    )").errors.empty());
}

TEST(QualifierConstMethods, ConstObjectCannotCallNonConstMethod) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        class C { public: int32 v; void set(int32 x) { this->v = x; } };
        int32 main() { C c; const C* p = &c; p->set(1); return 0; }
    )"), "const"));
}

TEST(QualifierConstMethods, ConstMethodCannotWriteField) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        class C { public: int32 v; void set() const { this->v = 1; } };
        int32 main() { return 0; }
    )"), "const"));
}
```

E2E:

```cpp
TEST_F(QualifiersE2E, ConstMethodReadsState) {
    EXPECT_EQ(runQualifierSource(R"(
        class C { public: int32 v; int32 get() const { return this->v; } };
        int32 main() { C c; c.v = 7; const C* p = &c; return p->get() - 7; }
    )"), 0);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/bin/compiler_tests --gtest_filter='ParserTest.ParsesConstMemberFunction:QualifierConstMethods.*:QualifiersE2E.ConstMethodReadsState'`

Expected: FAIL because the parser rejects trailing `const` in a method
declaration.

- [ ] **Step 3: Implement const method parsing and checking**

Parse the trailing `const` before `;`/`{`, store it on the method signature,
and build the implicit `this` parameter type with the class qualifier.
Update method lookup/call checking to compare object constness. Reject
duplicate const/non-const overloads. Update out-of-class method matching to
compare the const flag.

- [ ] **Step 4: Run focused tests**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierConstMethods.*:QualifiersE2E.ConstMethod*:ParserTest.ParsesConstMemberFunction'`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/frontend/Parser.cpp src/ast/Type.h src/ast/Decl.h src/sema/SemanticAnalyzer.cpp tests/frontend/test_parser.cpp tests/sema/test_qualifiers.cpp tests/e2e/test_qualifiers.cpp
git commit -m "feat: add const member functions"
```

---

### Task 6: Const-aware built-in methods

**Files:**
- Modify: `src/sema/SemanticAnalyzer.cpp` (built-in method dispatch)
- Modify: `src/codegen/CodegenContext.cpp` (const `string.data()` return type)
- Test: `tests/sema/test_qualifiers.cpp`
- Test: `tests/e2e/test_qualifiers.cpp`

**Interfaces:**
- Consumes: const method call checking.
- Produces: a built-in method table entry with a mutating/read-only flag and a
  const-aware return type for `string.data()`.

- [ ] **Step 1: Write failing built-in const tests**

```cpp
TEST(QualifierBuiltins, ConstStringRejectsAppend) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        int32 main() {
            string s = string.new("a");
            const string cs = s;
            cs.append("b");
            return 0;
        }
    )"), "const"));
}

TEST(QualifierBuiltins, ConstStringDataIsConstCharPointer) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        int32 main() {
            string s = string.new("a");
            const string cs = s;
            char* p = cs.data();
            return 0;
        }
    )"), "const"));
}

TEST(QualifierBuiltins, SliceOfConstElementsRejectsWrite) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        int32 main() {
            int32 a[2] = {1, 2};
            const int32[] cs = a;
            cs[0] = 3;
            return cs[0];
        }
    )"), "const"));
}
```

E2E:

```cpp
TEST_F(QualifiersE2E, ConstStringDataIsReadOnlyAtRuntime) {
    EXPECT_EQ(runQualifierSource(R"(
        int32 main() {
            string s = string.new("abc");
            const string cs = s;
            const char* p = cs.data();
            return p[0] == 'a' ? 0 : 1;
        }
    )"), 0);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierBuiltins.*:QualifiersE2E.ConstStringDataIsReadOnlyAtRuntime'`

Expected: FAIL because built-in dispatch currently ignores object constness.

- [ ] **Step 3: Implement const-aware built-in dispatch**

Add an explicit read-only/mutating classification for built-ins, including
`string`/`str`/`string_view`/`slice`/`optional`/`result`. Reject mutating
built-ins on const objects. Make `string.data()` return `const char*` when the
receiver is const, and keep `char*` for mutable receivers. Do not make
`const slice<T>` elements const.

- [ ] **Step 4: Run focused plus string/stdlib tests**

Run: `./build/bin/compiler_tests --gtest_filter='QualifierBuiltins.*:QualifiersE2E.*:StringE2E.*:Stdlib*'`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/sema/SemanticAnalyzer.cpp src/codegen/CodegenContext.cpp tests/sema/test_qualifiers.cpp tests/e2e/test_qualifiers.cpp
git commit -m "feat: make built-in method dispatch const-aware"
```

---

### Task 7: Migration and full verification

**Files:**
- Modify: `libs/std/**` only where stricter const/pointer rules expose real bugs
- Modify: `examples/**` only where they relied on discarded const
- Modify: `TODO.md`
- Modify: `Progress.md`
- Test: full suite and examples

**Interfaces:**
- Consumes all prior tasks.
- Produces a green const-correct compiler baseline.

- [ ] **Step 1: Build and run the full suite**

Run: `cmake --build build -j12 && ctest --test-dir build -j8 --output-on-failure`

Expected: all tests pass. Fix only real regressions; do not weaken the new
diagnostics to preserve old permissive tests.

- [ ] **Step 2: Run the feature examples**

Run: `./build/bin/cdd examples/all_features.cdd -o /tmp/all_features_qualifiers && /tmp/all_features_qualifiers | tail -1`

Expected: `ALL FEATURES OK`.

- [ ] **Step 3: Re-run the original reproduction cases**

Run the probe cases from the design's failure table through
`build/bin/cdd -S -O 2`. Expected:

- const discard, const object member writes, const pointer member writes,
  const object to mutable pointer, and const alias writes all fail with
  `const` diagnostics;
- `const S` with a pointer member still allows writing the pointee;
- copy-initialization from const aggregates still succeeds;
- `int32* const` diagnostics print `int32* const`;
- `const int32*` diagnostics print `const int32*`.

- [ ] **Step 4: Update progress docs**

Add a `TODO.md` snapshot and a `Progress.md` section with:
- new type-core/const behavior
- test count
- any migration fixes
- exact commits

- [ ] **Step 5: Commit**

```bash
git add libs examples TODO.md Progress.md
git commit -m "test: migrate const and pointer qualifier semantics"
```

---

## Self-review notes

- **Spec coverage:** Tasks 1-6 cover qualifier model, C pointer conversion,
  const lvalue propagation, aggregate modifiability, const member functions,
  and built-in const dispatch. Volatile/restrict are intentionally deferred to
  the next plan; references are deferred to the third plan.
- **Step scan:** Each task has a failing test, a focused run, implementation,
  a passing run, and a commit.
- **Type consistency:** All tasks use `TypeUtils` helpers from Task 1 and
  `qualifyMemberLValue` from Task 4. Signature helpers from Task 2 are used by
  the method work in Task 5.
- **Review Focus:** Multi-level pointers (Task 3), const pointer members
  (Task 4), aggregate assignment (Task 4), pointer spelling/mangling (Tasks
  2/3), and built-in const (Task 6) each have a test.
