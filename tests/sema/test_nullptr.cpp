// `null` must be a distinct null-pointer literal, not the integer constant 0.
// Integer 0 keeps its C null-pointer-constant meaning.
#include <gtest/gtest.h>

#include <string>
#include <vector>

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
    Lexer lexer("nullptr.cdd", source);
    Parser parser(lexer.tokenize());
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

TEST(NullLiteral, DoesNotConvertToIntegerOrBool) {
    auto intInit = analyzeSource("int32 main() { int32 x = null; return x; }");
    EXPECT_TRUE(hasError(intInit, "nullptr") || hasError(intInit, "incompatible"));
    auto boolInit = analyzeSource("int32 main() { bool b = null; return b ? 1 : 0; }");
    EXPECT_TRUE(hasError(boolInit, "nullptr") || hasError(boolInit, "incompatible"));
    auto floatInit = analyzeSource("int32 main() { float64 f = null; return 0; }");
    EXPECT_TRUE(hasError(floatInit, "nullptr") || hasError(floatInit, "incompatible"));
    auto intReturn = analyzeSource("int32 f() { return null; } int32 main() { return f(); }");
    EXPECT_TRUE(hasError(intReturn, "nullptr") || hasError(intReturn, "incompatible"));
}

TEST(NullLiteral, ConvertsToObjectVoidAndFunctionPointers) {
    auto result = analyzeSource(R"(
        void noop() {}
        int32 main() {
            int32* p = null;
            const int32* cp = null;
            void* v = null;
            void (*fp)() = null;
            return p == null && cp == null && v == null && fp == null ? 0 : 1;
        }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(NullLiteral, IntegerZeroRemainsNullPointerConstant) {
    auto result = analyzeSource(
        "int32 main() { int32* p = 0; return p == null ? 0 : 1; }");
    EXPECT_TRUE(result.errors.empty());
}

TEST(NullLiteral, OverloadPicksPointerOverIntegerAndBool) {
    auto pointerVsInt = analyzeSource(R"(
        int32 pick(int32 x) { return 1; }
        int32 pick(int32* p) { return 2; }
        int32 main() { return pick(null); }
    )");
    EXPECT_TRUE(pointerVsInt.errors.empty());
    auto pointerVsBool = analyzeSource(R"(
        int32 pick(bool b) { return 1; }
        int32 pick(int32* p) { return 2; }
        int32 main() { return pick(null); }
    )");
    EXPECT_TRUE(pointerVsBool.errors.empty());
}

TEST(NullLiteral, VoidAndObjectPointerOverloadsAreAmbiguous) {
    auto result = analyzeSource(R"(
        int32 pick(void* p) { return 1; }
        int32 pick(int32* p) { return 2; }
        int32 main() { return pick(null); }
    )");
    EXPECT_TRUE(hasError(result, "ambiguous"));
}

TEST(NullLiteral, BooleanParameterDoesNotAcceptNull) {
    auto result = analyzeSource(
        "int32 take(bool b) { return b ? 1 : 0; } int32 main() { return take(null); }");
    EXPECT_TRUE(hasError(result, "no matching") || hasError(result, "nullptr"));
}

TEST(NullLiteral, ComparisonRules) {
    auto ok = analyzeSource(R"(
        int32 main() {
            int32* p = null;
            return p == null && null == p && null == null && null == 0 && 0 == null ? 0 : 1;
        }
    )");
    EXPECT_TRUE(ok.errors.empty());
    auto bad = analyzeSource("int32 main() { return null == 1 ? 0 : 1; }");
    EXPECT_TRUE(hasError(bad, "comparison") || hasError(bad, "nullptr"));
}

TEST(NullLiteral, TernaryWithPointerProducesPointer) {
    auto ok = analyzeSource(R"(
        int32 main() {
            bool c = true;
            int32* q = null;
            int32* p = c ? null : q;
            return p == null ? 0 : 1;
        }
    )");
    EXPECT_TRUE(ok.errors.empty());
    auto bad = analyzeSource(
        "int32 main() { bool c = true; int32 x = c ? null : 1; return x; }");
    EXPECT_TRUE(hasError(bad, "ternary") || hasError(bad, "nullptr"));
}

TEST(NullLiteral, NullDefaultArgumentOnMethod) {
    auto result = analyzeSource(R"(
        class CNullDefault {
        public:
            int32 read(int32* p = null) { return p == null ? 0 : 1; }
        };
        int32 main() { CNullDefault c; return c.read(); }
    )");
    EXPECT_TRUE(result.errors.empty());
}

TEST(NullLiteral, NullDefaultArgumentOnlyForPointers) {
    auto ok = analyzeSource(R"(
        int32 read(int32* p = null) { return p == null ? 0 : 1; }
        int32 main() { return read(); }
    )");
    EXPECT_TRUE(ok.errors.empty());
    auto bad = analyzeSource(R"(
        int32 read(int32 x = null) { return x; }
        int32 main() { return read(); }
    )");
    EXPECT_TRUE(hasError(bad, "nullptr") || hasError(bad, "default"));
}
