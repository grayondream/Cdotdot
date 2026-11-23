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
