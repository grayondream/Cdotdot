#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "ast/Decl.h"
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
    Lexer lexer("references.cdd", source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    for (const auto& error : parser.getErrors())
        result.errors.push_back(error.message);
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

} // namespace

TEST(ReferencePlacement, ParsesAndTypesLocalReference) {
    auto result = analyzeSource("int32 main() { int32 x = 1; int32& r = x; return r; }");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferencePlacement, RejectsRvalueReference) {
    auto result = analyzeSource("int32 main() { int32&& r = 1; return r; }");
    EXPECT_TRUE(hasError(result, "rvalue reference"));
}

TEST(ReferencePlacement, RejectsReferenceArray) {
    auto result = analyzeSource("int32 main() { int32 x = 1; int32& a[2] = {x, x}; return 0; }");
    EXPECT_TRUE(hasError(result, "reference"));
}

TEST(ReferencePlacement, RejectsReferenceToReference) {
    auto result = analyzeSource("int32 main() { int32 x = 1; int32& r = x; int32& &rr = r; return 0; }");
    EXPECT_TRUE(hasError(result, "reference to a reference"));
}

TEST(ReferencePlacement, RejectsUnionReferenceMember) {
    auto result = analyzeSource("union U { int32& r; }; int32 main() { return 0; }");
    EXPECT_TRUE(hasError(result, "union"));
}

TEST(ReferencePlacement, RejectsGlobalReference) {
    auto result = analyzeSource("int32 g; int32& r = g; int32 main() { return 0; }");
    EXPECT_TRUE(hasError(result, "global"));
}

TEST(ReferencePlacement, RejectsAutoReference) {
    auto result = analyzeSource("int32 main() { int32 x = 1; auto& r = x; return 0; }");
    EXPECT_TRUE(hasError(result, "auto"));
}

TEST(ReferenceBinding, NonConstRefRejectsConstLvalue) {
    auto result = analyzeSource("int32 main() { const int32 x = 1; int32& r = x; return r; }");
    EXPECT_TRUE(hasError(result, "const"));
}

TEST(ReferenceBinding, NonConstRefRejectsRvalue) {
    auto result = analyzeSource("int32 main() { int32& r = 1; return r; }");
    EXPECT_TRUE(hasError(result, "rvalue"));
}

TEST(ReferenceBinding, ConstRefAcceptsRvalue) {
    auto result = analyzeSource("int32 main() { const int32& r = 1; return r; }");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferenceBinding, VolatileRefCanAddButNotDropVolatile) {
    auto ok = analyzeSource("int32 main() { int32 x = 1; volatile int32& r = x; return r; }");
    EXPECT_TRUE(ok.errors.empty());
    auto bad = analyzeSource("int32 main() { volatile int32 x = 1; int32& r = x; return r; }");
    EXPECT_TRUE(hasError(bad, "volatile"));
}

TEST(ReferenceBinding, ReferenceToPointerKeepsPointerObjectQualifiers) {
    auto ok = analyzeSource("int32 main() { int32 x = 1; int32* p = &x; int32*& r = p; *r = 2; return x; }");
    EXPECT_TRUE(ok.errors.empty());
    auto bad = analyzeSource("int32 main() { int32 x = 1; int32* const p = &x; int32*& r = p; return 0; }");
    EXPECT_TRUE(hasError(bad, "const"));
}

TEST(ReferenceBinding, ReferenceParameterRejectsTemporary) {
    auto result = analyzeSource("void f(int32& r) { r = 1; } int32 main() { f(1); return 0; }");
    EXPECT_TRUE(hasError(result, "rvalue"));
}

TEST(ReferenceBinding, ConstReferenceParameterAcceptsTemporary) {
    auto result = analyzeSource("int32 f(const int32& r) { return r; } int32 main() { return f(1) - 1; }");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferenceMembers, MemberWithoutInitializerRejected) {
    auto result = analyzeSource("struct SNoInitRef { int32& r; }; int32 main() { SNoInitRef s; return 0; }");
    EXPECT_TRUE(hasError(result, "reference"));
}

TEST(ReferenceMembers, EveryReferenceMemberMustBeBound) {
    auto result = analyzeSource(R"(
        struct SBindRef { int32 x; int32& r; };
        int32 main() { int32 a = 1; SBindRef s = {a}; return 0; }
    )");
    EXPECT_TRUE(hasError(result, "reference"));
}

TEST(ReferenceMembers, AggregateAssignmentWithReferenceRejected) {
    auto result = analyzeSource(R"(
        struct SAggAssign { int32& r; };
        int32 main() { int32 a = 1, b = 2; SAggAssign x = {a}; SAggAssign y = {b}; x = y; return 0; }
    )");
    EXPECT_TRUE(hasError(result, "reference"));
}

TEST(ReferenceMembers, MemberAssignmentWritesReferent) {
    auto result = analyzeSource(R"(
        struct SMemberAssign { int32& r; };
        int32 main() { int32 a = 1; SMemberAssign s = {a}; s.r = 2; return a; }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferenceMembers, ReferenceMemberFromTemporaryRejected) {
    auto result = analyzeSource(
        "struct STempRef { int32& r; }; int32 main() { STempRef s = {1}; return 0; }");
    EXPECT_TRUE(hasError(result, "temporary") || hasError(result, "rvalue"));
}

TEST(ReferenceSignature, PointerAndReferenceAreDistinctOverloads) {
    auto result = analyzeSource(R"(
        int32 f(int32* p) { return 1; }
        int32 f(int32& r) { return 2; }
        int32 main() { int32 x = 0; return f(&x) + f(x) - 3; }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferenceSignature, ConstReferenceIsDistinctOverload) {
    auto result = analyzeSource(R"(
        int32 f(int32& r) { return 1; }
        int32 f(const int32& r) { return 2; }
        int32 main() { int32 x = 0; return f(x) + f(1) - 3; }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferenceSignature, ValueAndReferenceParametersAreDuplicate) {
    auto result = analyzeSource(R"(
        void f(int32 x) {}
        void f(int32& r) {}
        int32 main() { return 0; }
    )");
    EXPECT_TRUE(hasError(result, "redefinition") || hasError(result, "duplicate"));
}

TEST(ReferenceTemplates, DeducesLvalueReference) {
    auto result = analyzeSource(R"(
        template<typename T> T identity_ref(T& value) { return value; }
        int32 main() { int32 x = 4; return identity_ref(x) - 4; }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferenceTemplates, ConstReferenceAcceptsRvalue) {
    auto result = analyzeSource(R"(
        template<typename T> T read_ref(const T& value) { return value; }
        int32 main() { return read_ref(9) - 9; }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferenceTemplates, LvalueReferenceRejectsRvalue) {
    auto result = analyzeSource(R"(
        template<typename T> T read_ref(T& value) { return value; }
        int32 main() { return read_ref(9); }
    )");
    EXPECT_TRUE(hasError(result, "rvalue") || hasError(result, "cannot deduce"));
}

TEST(ReferenceBinding, ConstRefRejectsIncompatibleRvalue) {
    auto stringToInt = analyzeSource(
        "int32 main() { const int32& r = \"hello\"; return r; }");
    EXPECT_TRUE(hasError(stringToInt, "incompatible") || hasError(stringToInt, "rvalue"));
    auto recordMismatch = analyzeSource(R"(
        struct RIncompatA { int32 a; };
        struct RIncompatB { int32 a; int32 b; };
        RIncompatB make_b() { RIncompatB v = {1, 2}; return v; }
        int32 main() { const RIncompatA& r = make_b(); return r.a; }
    )");
    EXPECT_TRUE(hasError(recordMismatch, "incompatible") ||
                hasError(recordMismatch, "rvalue"));
}

TEST(ReferenceBinding, RejectsVoidReferent) {
    auto result = analyzeSource("int32 main() { const void& r = 1; return 0; }");
    EXPECT_TRUE(hasError(result, "void"));
}

TEST(ReferenceBinding, ConstRefAllowsArithmeticConversionRvalue) {
    auto result = analyzeSource(
        "int32 main() { const float64& r = 3; return r > 2.0 ? 0 : 1; }");
    EXPECT_TRUE(result.errors.empty());
}

TEST(ReferenceMembers, GlobalAggregateWithReferenceRejected) {
    auto result = analyzeSource(R"(
        struct SGlobalRef { int32& r; };
        int32 g = 1;
        SGlobalRef gs = {g};
        int32 main() { return 0; }
    )");
    EXPECT_TRUE(hasError(result, "automatic storage") || hasError(result, "reference"));
}

TEST(ReferenceMembers, StaticLocalAggregateWithReferenceRejected) {
    auto result = analyzeSource(R"(
        struct SStaticRef { int32& r; };
        int32 main() { int32 x = 1; static SStaticRef s = {x}; return 0; }
    )");
    EXPECT_TRUE(hasError(result, "static"));
}

TEST(ReferenceDefaults, ReferenceParameterDefaultRejected) {
    auto result = analyzeSource(
        "int32 g(int32& r = 5) { return r; } int32 main() { return 0; }");
    EXPECT_TRUE(hasError(result, "default"));
}

TEST(ReferenceMethods, DuplicateMethodNamesAreDiagnosed) {
    auto result = analyzeSource(R"(
        class CMethodDup {
        public:
            int32 f(int32 x) { return x; }
            int32 f(int64 x) { return (int32)x; }
        };
        int32 main() { return 0; }
    )");
    EXPECT_TRUE(hasError(result, "overload") || hasError(result, "redefinition"));
}

TEST(ReferenceMembers, NestedReferenceMemberTemporaryRejected) {
    auto result = analyzeSource(R"(
        struct RefInnerTemp { const int32& r; };
        struct RefOuterTemp { RefInnerTemp in; };
        int32 main() { RefOuterTemp o = {{5}}; return 0; }
    )");
    EXPECT_TRUE(hasError(result, "temporary") || hasError(result, "rvalue"));
}

TEST(ReferenceMembers, DesignatedReferenceMemberTemporaryRejected) {
    auto result = analyzeSource(R"(
        struct RefDesigTemp { int32 x; const int32& r; int32 y; };
        int32 main() { int32 z = 2; RefDesigTemp s = {.r = 5, .y = z}; return 0; }
    )");
    EXPECT_TRUE(hasError(result, "temporary") || hasError(result, "rvalue"));
}

TEST(ReferenceSignature, ConstReferenceBeatsWideningForExactLvalue) {
    auto result = analyzeSource(R"(
        int32 pick(const int32& r) { return 1; }
        int32 pick(int64 v) { return 2; }
        int32 main() { int32 x = 0; return pick(x) - 1; }
    )");
    EXPECT_TRUE(result.errors.empty());
}
