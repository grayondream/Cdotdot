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
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"

class QualifiersE2E : public ::testing::Test {
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

static int runQualifierSource(const std::string& source) {
    Lexer lexer("qualifiers_e2e.cdd", source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return -1;

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.getErrors().empty()) return -1;

    CodegenContext ctx;
    ctx.setSourceFile("qualifiers_e2e.cdd");
    ast->codegen(ctx);
    ctx.finalizeDebugInfo();

    std::string ve;
    llvm::raw_string_ostream vs(ve);
    if (llvm::verifyModule(ctx.getModule(), &vs)) return -1;

    auto jtmb = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!jtmb) return -1;
    auto jit = llvm::orc::LLJITBuilder()
        .setJITTargetMachineBuilder(std::move(*jtmb))
        .create();
    if (!jit) return -1;

    auto ts = llvm::orc::ThreadSafeModule(ctx.takeModule(), ctx.takeContext());
    if (auto e = (*jit)->addIRModule(std::move(ts))) return -1;

    auto sym = (*jit)->lookup("main");
    if (!sym) return -1;
    return ((int (*)())(intptr_t)sym->getValue())();
}

TEST_F(QualifiersE2E, ConstObjectPointerMemberKeepsMutablePointee) {
    EXPECT_EQ(runQualifierSource(R"(
        struct QualifierE2ES { int32* p; };
        int32 main() {
            int32 x = 1;
            const QualifierE2ES s = {&x};
            *s.p = 42;
            return x - 42;
        }
    )"), 0);
}

TEST_F(QualifiersE2E, ConstMethodReadsState) {
    EXPECT_EQ(runQualifierSource(R"(
        class QualifierE2ECM {
        public:
            int32 v;
            int32 get() const { return this->v; }
        };
        int32 main() {
            QualifierE2ECM c;
            c.v = 7;
            const QualifierE2ECM* p = &c;
            return p->get() - 7;
        }
    )"), 0);
}

TEST_F(QualifiersE2E, ConstClassFieldRead) {
    EXPECT_EQ(runQualifierSource(R"(
        class QualifierE2ECFR {
        public:
            int32 v;
        };
        int32 main() {
            QualifierE2ECFR c;
            c.v = 7;
            const QualifierE2ECFR* p = &c;
            return p->v - 7;
        }
    )"), 0);
}

TEST_F(QualifiersE2E, ConstStringDataIsReadOnlyAtRuntime) {
    EXPECT_EQ(runQualifierSource(R"(
        int32 main() {
            string s = string.new("abc");
            const string cs = s;
            const char* p = cs.data();
            return p[0] == 'a' ? 0 : 1;
        }
    )"), 0);
}
