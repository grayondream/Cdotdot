#include <gtest/gtest.h>

#include "codegen/CodegenContext.h"
#include "frontend/Lexer.h"
#include "frontend/Parser.h"
#include "sema/SemanticAnalyzer.h"
#include "support/Log.h"

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include <spdlog/spdlog.h>

class ReferencesE2E : public ::testing::Test {
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
    Lexer lexer("references_e2e.cdd", source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return -1;
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.getErrors().empty()) return -1;
    CodegenContext ctx;
    ctx.setSourceFile("references_e2e.cdd");
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


static std::string compileIR(const std::string& source) {
    Lexer lexer("references_ir.cdd", source);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return "";
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.getErrors().empty()) return "";
    CodegenContext ctx;
    ctx.setSourceFile("references_ir.cdd");
    ast->codegen(ctx);
    ctx.finalizeDebugInfo();
    std::string errs;
    llvm::raw_string_ostream es(errs);
    if (llvm::verifyModule(ctx.getModule(), &es)) return "";
    std::string ir;
    llvm::raw_string_ostream os(ir);
    ctx.getModule().print(os, nullptr);
    return ir;
}

TEST_F(ReferencesE2E, LocalReadWriteAndAddress) {
    EXPECT_EQ(runSource(R"(
        int32 main() {
            int32 x = 1;
            int32& r = x;
            r = 2;
            int32* p = &r;
            *p = 3;
            return x - 3;
        }
    )"), 0);
}

TEST_F(ReferencesE2E, SizeofAndAlignofUseReferent) {
    EXPECT_EQ(runSource(R"(
        int32 main() {
            int32 x = 1;
            int32& r = x;
            return (sizeof(r) == sizeof(int32)) && (alignof(int32&) == alignof(int32)) ? 0 : 1;
        }
    )"), 0);
}

TEST_F(ReferencesE2E, ReferenceParameterWritesCaller) {
    EXPECT_EQ(runSource(R"(
        void set(int32& r) { r = 9; }
        int32 main() { int32 x = 1; set(x); return x - 9; }
    )"), 0);
}

TEST_F(ReferencesE2E, ReferenceReturnAliasesCallerObject) {
    EXPECT_EQ(runSource(R"(
        int32& pick(int32& r) { return r; }
        int32 main() { int32 x = 1; pick(x) = 7; return x - 7; }
    )"), 0);
}

TEST_F(ReferencesE2E, ConstReferenceBindsRvalueTemporary) {
    EXPECT_EQ(runSource(R"(
        int32 main() { const int32& r = 42; return r - 42; }
    )"), 0);
}

TEST_F(ReferencesE2E, ConstReferenceParameterBindsTemporary) {
    EXPECT_EQ(runSource(R"(
        int32 read(const int32& r) { return r; }
        int32 main() { return read(17) - 17; }
    )"), 0);
}

TEST_F(ReferencesE2E, ReferenceToPointerRebindsPointer) {
    EXPECT_EQ(runSource(R"(
        int32 main() {
            int32 a = 1, b = 2;
            int32* p = &a;
            int32*& r = p;
            r = &b;
            return *p - 2;
        }
    )"), 0);
}

TEST_F(ReferencesE2E, ReferenceLocalUsesPointerSizedSlot) {
    auto ir = compileIR("int32 main() { int32 x = 1; int32& r = x; return r; }");
    ASSERT_FALSE(ir.empty());
    EXPECT_NE(ir.find("alloca ptr"), std::string::npos);
    EXPECT_NE(ir.find("store ptr"), std::string::npos);
}

TEST_F(ReferencesE2E, ReferenceMemberAliasesExternalObject) {
    EXPECT_EQ(runSource(R"(
        struct HolderAlias { int32& r; };
        int32 main() {
            int32 x = 1;
            HolderAlias h = {x};
            h.r = 5;
            return x - 5;
        }
    )"), 0);
}

TEST_F(ReferencesE2E, AggregateCopyCopiesReferenceBinding) {
    EXPECT_EQ(runSource(R"(
        struct HolderCopy { int32& r; };
        int32 main() {
            int32 x = 1;
            HolderCopy a = {x};
            HolderCopy b = a;
            b.r = 7;
            return x - 7;
        }
    )"), 0);
}

TEST_F(ReferencesE2E, TemplateReferenceWritesThrough) {
    EXPECT_EQ(runSource(R"(
        template<typename T> void set_ref(T& value, T next) { value = next; }
        int32 main() { int32 x = 1; set_ref(x, 6); return x - 6; }
    )"), 0);
}

TEST_F(ReferencesE2E, MemberOperatorSubscriptIsWritableReference) {
    EXPECT_EQ(runSource(R"(
        struct BufferMemberSub {
            int32 data[2];
            int32& operator[](int32 i) { return this->data[i]; }
        };
        int32 main() {
            BufferMemberSub b;
            b[0] = 5;
            return b.data[0] - 5;
        }
    )"), 0);
}

TEST_F(ReferencesE2E, FreeOperatorSubscriptIsWritableReference) {
    EXPECT_EQ(runSource(R"(
        struct BufferFreeSub { int32 data[2]; };
        int32& operator[](BufferFreeSub& b, int32 i) { return b.data[i]; }
        int32 main() {
            BufferFreeSub b;
            b[0] = 6;
            return b.data[0] - 6;
        }
    )"), 0);
}
