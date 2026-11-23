#include <gtest/gtest.h>

#include <string>

#include "codegen/CodegenContext.h"
#include "frontend/Lexer.h"
#include "frontend/Parser.h"
#include "sema/SemanticAnalyzer.h"
#include "support/Log.h"

#include "llvm/Support/raw_ostream.h"

#include <spdlog/spdlog.h>

class RestrictIR : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::off);
    }
};

static std::string compileQualifierIR(const std::string& source) {
    Lexer lexer("restrict.cdd", source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return "";
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.getErrors().empty()) return "";
    CodegenContext ctx;
    ctx.setSourceFile("restrict.cdd");
    ast->codegen(ctx);
    ctx.finalizeDebugInfo();
    std::string ir;
    llvm::raw_string_ostream os(ir);
    ctx.getModule().print(os, nullptr);
    return ir;
}

TEST_F(RestrictIR, RestrictParameterGetsNoAlias) {
    auto ir = compileQualifierIR(R"(
        int32 sum(int32* restrict p, int32 n) {
            int32 total = 0;
            for (int32 i = 0; i < n; ++i) total += p[i];
            return total;
        }
        int32 main() { int32 a[2] = {1, 2}; return sum(a, 2); }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_NE(ir.find("noalias"), std::string::npos);
    EXPECT_NE(ir.find("ptr noalias"), std::string::npos);
}

TEST_F(RestrictIR, RestrictReturnGetsNoAlias) {
    auto ir = compileQualifierIR(R"(
        int32* restrict pick(int32* p) { return p; }
        int32 main() { int32 x = 1; return *pick(&x); }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_NE(ir.find("define noalias ptr"), std::string::npos);
}

TEST_F(RestrictIR, PlainPointerGetsNoNoAlias) {
    auto ir = compileQualifierIR(R"(
        int32 sum(int32* p, int32 n) {
            int32 total = 0;
            for (int32 i = 0; i < n; ++i) total += p[i];
            return total;
        }
        int32 main() { int32 a[2] = {1, 2}; return sum(a, 2); }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_EQ(ir.find("noalias"), std::string::npos);
}

TEST_F(RestrictIR, TypedefRestrictParameterGetsNoAlias) {
    auto ir = compileQualifierIR(R"(
        using RestrictIntPtr = int32* restrict;
        int32 read(RestrictIntPtr p) { return *p; }
        int32 main() { int32 x = 1; return read(&x); }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_NE(ir.find("ptr noalias"), std::string::npos);
}
