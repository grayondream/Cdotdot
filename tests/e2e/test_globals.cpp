// 全局变量 codegen：无初始化器的全局变量必须是零初始化定义（tentative
// definition 语义），而非 external 声明。含全局 slice 的 D3 零初始化（TYP-12）。
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

class GlobalsE2E : public ::testing::Test {
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

static int runSource(const std::string& source, const std::string& filename) {
    Lexer lexer(filename, source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!ast || !parser.getErrors().empty()) return -1;

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.getErrors().empty()) return -1;

    CodegenContext ctx;
    ctx.setSourceFile(filename);
    ast->codegen(ctx);
    ctx.finalizeDebugInfo();

    std::string ve;
    llvm::raw_string_ostream vs(ve);
    if (llvm::verifyModule(ctx.getModule(), &vs)) return -1;

    auto jtmb = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!jtmb) return -1;
    auto jit = llvm::orc::LLJITBuilder().setJITTargetMachineBuilder(std::move(*jtmb)).create();
    if (!jit) return -1;

    auto ts = llvm::orc::ThreadSafeModule(ctx.takeModule(), ctx.takeContext());
    if (auto e = (*jit)->addIRModule(std::move(ts))) return -1;

    auto entry = (*jit)->lookup("main");
    if (!entry) return -1;
    using MainFn = int (*)();
    auto fn = reinterpret_cast<MainFn>(entry->getValue());
    if (!fn) return -1;
    return fn();
}

// An uninitialized global is a zero-initialized definition, not an
// external declaration (the old bug: `int32 g;` emitted no storage).
TEST_F(GlobalsE2E, UninitializedGlobalZeroInitialized) {
    EXPECT_EQ(runSource(R"(
        int32 g;
        int32 main() { return g; }
    )", "test_global_zero.cdd"), 0);
}

// An initialized global keeps its value.
TEST_F(GlobalsE2E, InitializedGlobalValue) {
    EXPECT_EQ(runSource(R"(
        int32 g = 5;
        int32 main() { return g - 5; }
    )", "test_global_init.cdd"), 0);
}

// Writes to a global are visible across calls.
TEST_F(GlobalsE2E, GlobalWriteVisible) {
    EXPECT_EQ(runSource(R"(
        int32 counter;
        void bump() { counter = counter + 1; }
        int32 main() {
            bump();
            bump();
            return counter == 2 ? 0 : 1;
        }
    )", "test_global_write.cdd"), 0);
}

// TYP-12 D3 via the global fix: an uninitialized global slice is the
// empty slice {nullptr, 0} (restored from TYP-12 Task 6).
TEST_F(GlobalsE2E, GlobalSliceZeroInitialized) {
    EXPECT_EQ(runSource(R"(
        int32[] g;
        int32 main() { return g.len == 0 ? 0 : 1; }
    )", "test_global_slice.cdd"), 0);
}

// P0: a global initializer that needs runtime evaluation must be diagnosed;
// it must not be silently zero-initialized or crash codegen.
TEST_F(GlobalsE2E, NonConstantGlobalInitializerRejected) {
    Lexer lexer("test_global_dynamic_init.cdd",
        "int32 f() { return 7; }\n"
        "int32 g = f();\n"
        "int32 main() { return g; }\n");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    ASSERT_NE(ast, nullptr);
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    EXPECT_FALSE(analyzer.getErrors().empty());
}

// P0: a const global referenced by another global initializer is a constant
// expression; it must fold to its value, not emit ptrtoint(address).
TEST_F(GlobalsE2E, ConstGlobalReferenceFolds) {
    EXPECT_EQ(runSource(R"(
        const int32 a = 5;
        int32 b = a;
        int32 main() { return b - 5; }
    )", "test_global_const_ref.cdd"), 0);
}

// P0: a block-scope extern object declaration must not bind to a same-named
// function and emit a `.1` global.
TEST_F(GlobalsE2E, BlockExternObjectCannotBindFunction) {
    Lexer lexer("test_block_extern_function.cdd",
        "int32 f(int32 x) { return x + 1; }\n"
        "int32 main() { extern int32 f; return f; }\n");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    ASSERT_NE(ast, nullptr);
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    EXPECT_FALSE(analyzer.getErrors().empty());
}

// P0: a file-scope extern object is a raw C declaration, not a zero-initialized
// CDD definition.
TEST_F(GlobalsE2E, FileScopeExternObjectIsDeclaration) {
    Lexer lexer("test_global_extern_object.cdd",
        "extern int32 cextern;\n"
        "int32 main() { return cextern; }\n");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    ASSERT_NE(ast, nullptr);
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    ASSERT_TRUE(analyzer.getErrors().empty());
    CodegenContext ctx;
    ctx.setSourceFile("test_global_extern_object.cdd");
    ast->codegen(ctx);
    llvm::GlobalVariable* gv =
        ctx.getModule().getGlobalVariable("cextern", /*AllowInternal=*/true);
    ASSERT_NE(gv, nullptr);
    EXPECT_TRUE(gv->isDeclaration());
    EXPECT_FALSE(gv->hasInitializer());
}
