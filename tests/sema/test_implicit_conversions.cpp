// Implicit conversion safety: pointers and integers are distinct conversion
// categories. Only the integer null pointer constant (literal 0) may flow into
// a pointer implicitly; pointer-to-integer and arbitrary integer-to-pointer
// conversions require an explicit cast.
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
    Lexer lexer("implicit_conversion.cdd", source);
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

bool hasAnyError(const AnalyzeResult& result) {
    return result.parseFailed || !result.errors.empty();
}

} // namespace

TEST(ImplicitConversion, PointerDoesNotConvertToIntegerOnInitialization) {
    auto result = analyzeSource(R"(
        int32 main() {
            int32 x = 1;
            int32* p = &x;
            int32 y = p;
            return y;
        }
    )");
    EXPECT_TRUE(hasAnyError(result));
    EXPECT_TRUE(hasError(result, "type mismatch") ||
                hasError(result, "incompatible") ||
                hasError(result, "cannot"));
}

TEST(ImplicitConversion, PointerDoesNotConvertToIntegerOnReturn) {
    auto result = analyzeSource(R"(
        int32 f() {
            int32 x = 1;
            int32* p = &x;
            return p;
        }
        int32 main() { return f(); }
    )");
    EXPECT_TRUE(hasAnyError(result));
    EXPECT_TRUE(hasError(result, "return type mismatch") ||
                hasError(result, "incompatible"));
}

TEST(ImplicitConversion, PointerDoesNotConvertToIntegerThroughTernary) {
    auto result = analyzeSource(R"(
        int32 main() {
            bool c = true;
            int32 x = 1;
            int32* p = &x;
            int32 y = c ? p : 0;
            return y;
        }
    )");
    EXPECT_TRUE(hasAnyError(result));
    EXPECT_TRUE(hasError(result, "type mismatch") ||
                hasError(result, "incompatible"));
}

TEST(ImplicitConversion, NonZeroIntegerDoesNotConvertToPointerThroughTernary) {
    auto result = analyzeSource(R"(
        int32 main() {
            bool c = true;
            int32 x = 1;
            int32* p = &x;
            int32* q = c ? p : 5;
            return q == p ? 0 : 1;
        }
    )");
    EXPECT_TRUE(hasAnyError(result));
    EXPECT_TRUE(hasError(result, "ternary") ||
                hasError(result, "non-zero integer") ||
                hasError(result, "incompatible"));
}

TEST(ImplicitConversion, NonZeroIntegerDoesNotConvertToPointerArgument) {
    auto result = analyzeSource(R"(
        int32 take(int32* p) { return p == nullptr ? 0 : 1; }
        int32 main() { return take(5); }
    )");
    EXPECT_TRUE(hasAnyError(result));
    EXPECT_TRUE(hasError(result, "non-zero integer") ||
                hasError(result, "no matching"));
}

TEST(ImplicitConversion, NonZeroIntegerDoesNotConvertToFunctionPointerParameter) {
    auto result = analyzeSource(R"(
        int32 main() {
            auto f = [](int32* p) { return p == nullptr ? 0 : 1; };
            return f(5);
        }
    )");
    EXPECT_TRUE(hasAnyError(result));
    EXPECT_TRUE(hasError(result, "non-zero integer") ||
                hasError(result, "indirect call") ||
                hasError(result, "incompatible"));
}

TEST(ImplicitConversion, NonZeroIntegerDoesNotConvertToClosurePointerParameter) {
    auto result = analyzeSource(R"(
        int32 main() {
            int32 captured = 1;
            auto f = [captured](int32* p) { return p == nullptr ? 0 : 1; };
            return f(5);
        }
    )");
    EXPECT_TRUE(hasAnyError(result));
    EXPECT_TRUE(hasError(result, "non-zero integer") ||
                hasError(result, "closure argument") ||
                hasError(result, "lambda argument") ||
                hasError(result, "incompatible"));
}

TEST(ImplicitConversion, NonZeroIntegerDoesNotConvertToAutoReturnPointer) {
    auto result = analyzeSource(R"(
        auto f(bool c) {
            int32 x = 1;
            int32* p = &x;
            if (c) return p;
            return 5;
        }
        int32 main() { return f(true) == nullptr ? 1 : 0; }
    )");
    EXPECT_TRUE(hasAnyError(result));
    EXPECT_TRUE(hasError(result, "inconsistent return types") ||
                hasError(result, "incompatible") ||
                hasError(result, "non-zero integer"));
}

TEST(ImplicitConversion, NullPointerConstantStillConvertsToPointer) {
    auto result = analyzeSource(R"(
        int32 main() {
            bool c = true;
            int32 x = 1;
            int32* p = &x;
            int32* a = 0;
            int32* b = c ? 0 : p;
            int32* d = c ? p : 0;
            int32 captured = 1;
            auto f = [](int32* q) { return q == nullptr ? 0 : 1; };
            auto g = [captured](int32* q) { return q == nullptr ? 0 : 1; };
            return a == nullptr && b == p && d == p && f(0) && g(0) ? 0 : 1;
        }
    )");
    EXPECT_TRUE(result.parseFailed == false);
    EXPECT_TRUE(result.errors.empty());
}

TEST(ImplicitConversion, PointerComparisonWithZeroStillWorks) {
    auto result = analyzeSource(R"(
        int32 main() {
            int32 x = 1;
            int32* p = &x;
            return p == 0 && 0 == p && p != 0 ? 0 : 1;
        }
    )");
    EXPECT_TRUE(result.parseFailed == false);
    EXPECT_TRUE(result.errors.empty());
}

TEST(ImplicitConversion, ExplicitPointerIntegerConversionsStillWork) {
    auto result = analyzeSource(R"(
        int32 main() {
            int32 x = 1;
            int32* p = &x;
            int32 y = (int32)p;
            int32* q = (int32*)y;
            int32 z = reinterpret_cast<int32>(p);
            return q == p && z == y ? 0 : 1;
        }
    )");
    EXPECT_TRUE(result.parseFailed == false);
    EXPECT_TRUE(result.errors.empty());
}
