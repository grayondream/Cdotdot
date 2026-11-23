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
