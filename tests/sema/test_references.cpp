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
