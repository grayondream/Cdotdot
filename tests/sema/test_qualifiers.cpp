#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "ast/Decl.h"
#include "ast/Mangle.h"
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
        struct QualifierSharedS { int32 x; };
        const QualifierSharedS* p;
        QualifierSharedS q;
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

TEST(QualifierPointers, HelperRejectsDrop) {
    EXPECT_FALSE(pointerConversionAllowed(parseTypeOnly("int32*"),
                                          parseTypeOnly("const int32*")));
    EXPECT_FALSE(pointerConversionAllowed(parseTypeOnly("const int32**"),
                                          parseTypeOnly("int32**")));
}

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
    EXPECT_TRUE(analyzeSource(
        "int32 main() { int32* p = 0; return p == null ? 0 : 1; }")
                    .errors.empty());
}

TEST(QualifierConstLValue, RejectsMemberWriteThroughConstObject) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct QCS1 { int32 x; };
        int32 main() { const QCS1 s = {1}; s.x = 2; return s.x; }
    )"), "const"));
}

TEST(QualifierConstLValue, RejectsMemberWriteThroughConstPointer) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct QCS2 { int32 x; };
        int32 main() { QCS2 s = {1}; const QCS2* p = &s; p->x = 2; return s.x; }
    )"), "const"));
}

TEST(QualifierConstLValue, ConstObjectPointerMemberIsConstPointer) {
    auto result = analyzeSource(R"(
        struct QCS3 { int32* p; };
        int32 main() {
            int32 x = 1;
            const QCS3 s = {&x};
            *s.p = 2;
            return x;
        }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(QualifierConstLValue, RejectsAddressOfConstObjectToMutablePointer) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct QCS4 { int32 x; };
        int32 main() { const QCS4 s = {1}; QCS4* p = &s; return 0; }
    )"), "const"));
}

TEST(QualifierConstLValue, RejectsWholeConstAggregateAssignment) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct QCS5 { int32 x; };
        int32 main() { const QCS5 a = {1}; QCS5 b = {2}; a = b; return 0; }
    )"), "const"));
}

TEST(QualifierConstLValue, AllowsCopyInitializationFromConstAggregate) {
    auto result = analyzeSource(R"(
        struct QCS6 { int32 x; };
        int32 main() { const QCS6 a = {1}; QCS6 b = a; b.x = 2; return b.x; }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(QualifierConstLValue, RejectsAggregateWithConstFieldAssignment) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        struct QCS7 { const int32 x; };
        int32 main() { QCS7 a = {1}; QCS7 b = {2}; a = b; return 0; }
    )"), "const"));
}

TEST(QualifierConstLValue, ConstAliasPreservesReadonly) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        using CI = const int32;
        int32 main() { CI x = 1; x = 2; return x; }
    )"), "const"));
}

TEST(QualifierConstMethods, ConstObjectCanCallConstMethod) {
    EXPECT_TRUE(analyzeSource(R"(
        class QCM1 { public: int32 v; int32 get() const { return this->v; } };
        int32 main() { QCM1 c; c.v = 1; const QCM1* p = &c; return p->get(); }
    )").errors.empty());
}

TEST(QualifierConstMethods, ConstObjectCannotCallNonConstMethod) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        class QCM2 { public: int32 v; void set(int32 x) { this->v = x; } };
        int32 main() { QCM2 c; const QCM2* p = &c; p->set(1); return 0; }
    )"), "const"));
}

TEST(QualifierConstMethods, ConstMethodCannotWriteField) {
    EXPECT_TRUE(hasError(analyzeSource(R"(
        class QCM3 { public: int32 v; void set() const { this->v = 1; } };
        int32 main() { return 0; }
    )"), "const"));
}
