// SEM-01/02: definite-assignment analysis (warnings only, never errors).
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "frontend/Lexer.h"
#include "frontend/Parser.h"
#include "sema/SemanticAnalyzer.h"

#include <spdlog/spdlog.h>

namespace {

class InitializationAnalysisTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

struct Result {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

Result analyze(const std::string& source) {
    Lexer lexer("init.c", source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    Result r;
    if (!ast) {
        r.errors.push_back("<parse failed>");
        return r;
    }
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    for (const auto& d : analyzer.getErrors()) r.errors.push_back(d.message);
    for (const auto& d : analyzer.getWarnings()) r.warnings.push_back(d.message);
    return r;
}

bool warnedAbout(const Result& r, const std::string& var) {
    for (const auto& w : r.warnings) {
        if (w.find("'" + var + "'") != std::string::npos) return true;
    }
    return false;
}

} // namespace

TEST_F(InitializationAnalysisTest, WarnsOnUninitializedRead) {
    auto r = analyze("int32 main() { int32 y; return y; }");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(warnedAbout(r, "y"));
}

TEST_F(InitializationAnalysisTest, NoWarningWhenInitialized) {
    auto r = analyze("int32 main() { int32 y = 3; return y; }");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());
}

TEST_F(InitializationAnalysisTest, NoWarningForParameters) {
    auto r = analyze("int32 f(int32 a) { return a + 1; } int32 main() { return f(2) - 3; }");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());
}

TEST_F(InitializationAnalysisTest, NoWarningForGlobals) {
    auto r = analyze("int32 g; int32 main() { return g; }");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());
}

TEST_F(InitializationAnalysisTest, BranchesJoinOnBothAssigned) {
    auto r = analyze(R"(
int32 main() {
    int32 a;
    int32 c = 1;
    if (c) { a = 1; } else { a = 2; }
    return a;
})");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_FALSE(warnedAbout(r, "a"));
}

TEST_F(InitializationAnalysisTest, WarnsWhenOnlyOneBranchAssigns) {
    auto r = analyze(R"(
int32 main() {
    int32 a;
    int32 c = 1;
    if (c) { a = 1; }
    return a;
})");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(warnedAbout(r, "a"));
}

TEST_F(InitializationAnalysisTest, LoopIsConservative) {
    // The body may run zero times, so `x` is not definitely assigned.
    auto r = analyze(R"(
int32 main() {
    int32 x;
    int32 c = 0;
    while (c) { x = 1; }
    return x;
})");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(warnedAbout(r, "x"));
}

TEST_F(InitializationAnalysisTest, DoWhileAssignsInBody) {
    auto r = analyze(R"(
int32 main() {
    int32 x;
    int32 c = 1;
    do { x = 2; c = 0; } while (c);
    return x;
})");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_FALSE(warnedAbout(r, "x"));
}

TEST_F(InitializationAnalysisTest, AddressTakenSuppressesWarning) {
    auto r = analyze(R"(
void set(int32* p) { *p = 7; }
int32 main() {
    int32 x;
    set(&x);
    return x;
})");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_FALSE(warnedAbout(r, "x"));
}

TEST_F(InitializationAnalysisTest, WarnsOnUninitializedPointerDeref) {
    auto r = analyze("int32 main() { int32* p; return *p; }");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(warnedAbout(r, "p"));
}

TEST_F(InitializationAnalysisTest, RejectsIncompleteByValueFields) {
    auto forward = analyze(
        "struct FA; struct FS { FA a; }; struct FA { int32 x; }; int32 main() { return 0; }");
    EXPECT_FALSE(forward.errors.empty());

    auto mutual = analyze(
        "struct MB; struct MA { MB b; }; struct MB { MA a; }; int32 main() { return 0; }");
    EXPECT_FALSE(mutual.errors.empty());

    auto pointer = analyze(
        "struct PA; struct PS { PA* p; }; struct PA { int32 x; }; int32 main() { return 0; }");
    EXPECT_TRUE(pointer.errors.empty()) << (pointer.errors.empty() ? "" : pointer.errors.front());
}

TEST_F(InitializationAnalysisTest, RejectsBraceElementTypeMismatch) {
    auto arrayResult = analyze(
        "int32 main() { int32 a[2] = { \"bad\", 2 }; return a[0]; }");
    EXPECT_FALSE(arrayResult.errors.empty());

    auto structResult = analyze(
        "struct S { int32 x; int32 y; }; "
        "int32 main() { S s = { \"bad\", 2 }; return s.x; }");
    EXPECT_FALSE(structResult.errors.empty());

    auto nestedResult = analyze(
        "struct S { int32 x; int32 y; }; "
        "int32 main() { S a[1] = { { \"bad\", 2 } }; return a[0].x; }");
    EXPECT_FALSE(nestedResult.errors.empty());
}

TEST_F(InitializationAnalysisTest, AssignedBeforeReadIsFine) {
    auto r = analyze("int32 main() { int32 y; y = 4; return y; }");
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(r.warnings.empty());
}
