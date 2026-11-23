// Runtime coverage for the distinct null-pointer literal.
#include <gtest/gtest.h>

#include "codegen/CodegenContext.h"
#include "frontend/Lexer.h"
#include "frontend/Parser.h"
#include "sema/SemanticAnalyzer.h"
#include "support/Log.h"

#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"

#include <spdlog/spdlog.h>

class NullLiteralE2E : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::off);
        static bool init = false;
        if (!init) {
            llvm::InitializeAllTargetInfos();
            llvm::InitializeAllTargets();
            llvm::InitializeAllTargetMCs();
            llvm::InitializeAllAsmParsers();
            llvm::InitializeAllAsmPrinters();
            init = true;
        }
    }
};

static int runSource(const std::string& source) {
    Lexer lexer("nullptr_e2e.cdd", source);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return -1;

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.getErrors().empty()) return -1;

    CodegenContext ctx;
    ctx.setSourceFile("nullptr_e2e.cdd");
    ast->codegen(ctx);
    ctx.finalizeDebugInfo();

    std::string verifyErrors;
    llvm::raw_string_ostream verifyStream(verifyErrors);
    if (llvm::verifyModule(ctx.getModule(), &verifyStream)) return -1;

    auto jitBuilder = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!jitBuilder) return -1;
    auto jit = llvm::orc::LLJITBuilder()
                   .setJITTargetMachineBuilder(std::move(*jitBuilder))
                   .create();
    if (!jit) return -1;

    auto ts = llvm::orc::ThreadSafeModule(ctx.takeModule(), ctx.takeContext());
    if (auto e = (*jit)->addIRModule(std::move(ts))) return -1;

    auto sym = (*jit)->lookup("main");
    if (!sym) return -1;
    return ((int (*)())(intptr_t)sym->getValue())();
}

TEST_F(NullLiteralE2E, OverloadPicksPointerOverInteger) {
    EXPECT_EQ(runSource(R"(
        int32 pick(int32 x) { return 1; }
        int32 pick(int32* p) { return 2; }
        int32 main() { return pick(null) - 2; }
    )"), 0);
}

TEST_F(NullLiteralE2E, NullPointerAssignmentAndComparison) {
    EXPECT_EQ(runSource(R"(
        int32 main() {
            int32* p = null;
            if (p != null) return 1;
            if (!(null == p)) return 2;
            if (!(null == null)) return 3;
            if (!(null == 0)) return 4;
            if (null) return 5;
            if (!(!null)) return 6;
            if (null && true) return 7;
            return 0;
        }
    )"), 0);
}

TEST_F(NullLiteralE2E, NullDefaultArgument) {
    EXPECT_EQ(runSource(R"(
        int32 read(int32* p = null) { return p == null ? 0 : 1; }
        int32 main() { return read(); }
    )"), 0);
}

TEST_F(NullLiteralE2E, TernaryNullProducesNullPointer) {
    EXPECT_EQ(runSource(R"(
        int32 main() {
            bool c = true;
            int32* q = null;
            int32* p = c ? null : q;
            return p == null ? 0 : 1;
        }
    )"), 0);
}

TEST_F(NullLiteralE2E, VoidPointerReturnAndFunctionPointerInit) {
    EXPECT_EQ(runSource(R"(
        void* empty() { return null; }
        void noop() {}
        int32 main() {
            if (empty() != null) return 1;
            void (*fp)() = null;
            if (fp != null) return 2;
            fp = noop;
            if (fp == null) return 3;
            return 0;
        }
    )"), 0);
}
