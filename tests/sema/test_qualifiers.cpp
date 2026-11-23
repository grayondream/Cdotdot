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
