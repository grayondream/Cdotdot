#include <gtest/gtest.h>

#include <string>

#include "codegen/CodegenContext.h"
#include "frontend/Lexer.h"
#include "frontend/Parser.h"
#include "sema/SemanticAnalyzer.h"
#include "support/Log.h"

#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include <spdlog/spdlog.h>

class QualifiersIR : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::off);
    }
};

static std::string compileQualifierIRImpl(const std::string& source,
                                          bool optimizeO2) {
    Lexer lexer("volatile.cdd", source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return "";
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.getErrors().empty()) return "";
    CodegenContext ctx;
    ctx.setSourceFile("volatile.cdd");
    ast->codegen(ctx);
    ctx.finalizeDebugInfo();

    if (optimizeO2) {
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;
        llvm::PassBuilder PB;
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
        llvm::ModulePassManager MPM =
            PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        MPM.run(ctx.getModule(), MAM);
    }

    std::string ir;
    llvm::raw_string_ostream os(ir);
    ctx.getModule().print(os, nullptr);
    return ir;
}

static std::string compileQualifierIR(const std::string& source) {
    return compileQualifierIRImpl(source, false);
}

static std::string compileQualifierIRAtO2(const std::string& source) {
    return compileQualifierIRImpl(source, true);
}

static size_t countSubstring(const std::string& hay, const std::string& needle) {
    size_t count = 0;
    for (size_t pos = 0; (pos = hay.find(needle, pos)) != std::string::npos;
         pos += needle.size()) {
        ++count;
    }
    return count;
}

TEST_F(QualifiersIR, ScalarVolatileLoadsAndStores) {
    auto ir = compileQualifierIR(R"(
        volatile int32 g;
        int32 main() {
            volatile int32 x = 1;
            x = 2;
            x = 3;
            g = 4;
            return x + g;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_GE(countSubstring(ir, "store volatile"), 4u);
    EXPECT_GE(countSubstring(ir, "load volatile"), 2u);
}

TEST_F(QualifiersIR, VolatileObjectMakesOrdinaryFieldsVolatile) {
    auto ir = compileQualifierIR(R"(
        struct VolatileFieldS { int32 x; };
        int32 main() {
            volatile struct VolatileFieldS s;
            s.x = 7;
            return s.x;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_GE(countSubstring(ir, "store volatile i32"), 1u);
    EXPECT_GE(countSubstring(ir, "load volatile i32"), 1u);
}

TEST_F(QualifiersIR, VolatileArrayElementsAreVolatile) {
    auto ir = compileQualifierIR(R"(
        int32 main() {
            volatile int32 a[2] = {1, 2};
            a[0] = 3;
            return a[0];
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_GE(countSubstring(ir, "store volatile i32"), 3u);
    EXPECT_GE(countSubstring(ir, "load volatile i32"), 1u);
}

TEST_F(QualifiersIR, VolatileAggregateDestinationUsesVolatileCopy) {
    auto ir = compileQualifierIR(R"(
        struct VolatileCopyS { int32 x; int32 y; };
        int32 main() {
            struct VolatileCopyS src = {1, 2};
            volatile struct VolatileCopyS dst;
            dst = src;
            return dst.x;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_GE(countSubstring(ir, "store volatile"), 1u);
}

TEST_F(QualifiersIR, VolatileAggregateSourceUsesVolatileRead) {
    auto ir = compileQualifierIR(R"(
        struct VolatileSourceS { int32 x; int32 y; };
        int32 main() {
            volatile struct VolatileSourceS src;
            struct VolatileSourceS dst;
            dst = src;
            return dst.x;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_GE(countSubstring(ir, "load volatile"), 1u);
}

TEST_F(QualifiersIR, OptimizationKeepsBothVolatileStores) {
    auto ir = compileQualifierIRAtO2(R"(
        int32 main() {
            volatile int32 x = 1;
            x = 2;
            x = 3;
            return 0;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_EQ(countSubstring(ir, "store volatile"), 3u);
}

TEST_F(QualifiersIR, PointeeVolatileDoesNotQualifyPointerVariable) {
    auto ir = compileQualifierIR(R"(
        volatile int32 g;
        int32 main() {
            volatile int32* p = &g;
            return *p;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_GE(countSubstring(ir, "load volatile i32"), 1u);
    EXPECT_EQ(countSubstring(ir, "load volatile ptr"), 0u);
}

TEST_F(QualifiersIR, PointerVariableVolatileDoesNotQualifyPointee) {
    auto ir = compileQualifierIR(R"(
        int32 g;
        int32 main() {
            int32* volatile p = &g;
            int32* q = p;
            return *q;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_GE(countSubstring(ir, "load volatile ptr"), 1u);
    EXPECT_EQ(countSubstring(ir, "load volatile i32"), 0u);
}

TEST_F(QualifiersIR, AtomicVolatileKeepsBothProperties) {
    auto ir = compileQualifierIR(R"(
        int32 main() {
            atomic volatile int32 x = 1;
            x = 2;
            return x;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_NE(ir.find("load atomic volatile"), std::string::npos);
    EXPECT_NE(ir.find("store atomic volatile"), std::string::npos);
}

TEST_F(QualifiersIR, VolatileBitFieldUsesVolatileRmw) {
    auto ir = compileQualifierIR(R"(
        struct VolatileBits { volatile int32 x : 3; };
        int32 main() {
            struct VolatileBits b;
            b.x = 2;
            return b.x;
        }
    )");
    ASSERT_FALSE(ir.empty());
    EXPECT_GE(countSubstring(ir, "load volatile i32"), 1u);
    EXPECT_GE(countSubstring(ir, "store volatile i32"), 1u);
}
