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
    Lexer lexer("restrict.cdd", source);
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

} // namespace

TEST(RestrictPlacement, LeadingRestrictOnNonPointerRejected) {
    auto result = analyzeSource("restrict int32 g; int32 main() { return 0; }");
    EXPECT_FALSE(result.parseFailed);
    EXPECT_TRUE(hasError(result, "restrict"));
}

TEST(RestrictPlacement, TrailingRestrictOnNonPointerRejected) {
    auto result = analyzeSource("int32 restrict g; int32 main() { return 0; }");
    EXPECT_FALSE(result.parseFailed);
    EXPECT_TRUE(hasError(result, "restrict"));
}

TEST(RestrictPlacement, PointerVariableAccepted) {
    auto result = analyzeSource(
        "int32 main() { int32 x = 1; int32* restrict p = &x; return *p; }");
    EXPECT_TRUE(result.errors.empty());
}

TEST(RestrictPlacement, TypedefPointerAccepted) {
    auto result = analyzeSource(
        "using RP = int32* restrict; RP p; int32 main() { return 0; }");
    EXPECT_TRUE(result.errors.empty());
}

TEST(RestrictPlacement, FunctionPointerRejected) {
    auto result = analyzeSource(
        "int32 (*restrict fp)(int32); int32 main() { return 0; }");
    EXPECT_TRUE(hasError(result, "restrict"));
}

TEST(RestrictPlacement, GlobalAndMemberPointersAccepted) {
    auto result = analyzeSource(R"(
        int32* restrict g;
        struct RestrictMemberS { int32* restrict p; };
        int32 main() { return 0; }
    )");
    EXPECT_TRUE(result.errors.empty());
}
