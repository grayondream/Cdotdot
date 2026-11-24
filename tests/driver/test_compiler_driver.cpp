#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#include "driver/CompilerDriver.h"

#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"

#include <spdlog/spdlog.h>

class CompilerDriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool init = false;
        if (!init) {
            llvm::InitializeAllTargetInfos();
            llvm::InitializeAllTargets();
            llvm::InitializeAllTargetMCs();
            llvm::InitializeAllAsmParsers();
            llvm::InitializeAllAsmPrinters();
            init = true;
        }
        driver = CompilerDriver();
    }

    CompilerDriver driver;
};

TEST_F(CompilerDriverTest, ParseCompileOnlyFlag) {
    const char* argv[] = {"cdd", "-c", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getCompileOnly());
}

TEST_F(CompilerDriverTest, ParseOutputFlag) {
    const char* argv[] = {"cdd", "-o", "out.o", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    EXPECT_EQ(driver.getOutputFile(), "out.o");
}

TEST_F(CompilerDriverTest, ParseSingleModulePath) {
    const char* argv[] = {"cdd", "-M", "libs", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    ASSERT_EQ(driver.getModulePaths().size(), 1u);
    EXPECT_EQ(driver.getModulePaths()[0], "libs");
}

TEST_F(CompilerDriverTest, ParseMultipleModulePaths) {
    const char* argv[] = {"cdd", "-M", "libs", "-M", "vendored", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    ASSERT_EQ(driver.getModulePaths().size(), 2u);
    EXPECT_EQ(driver.getModulePaths()[0], "libs");
    EXPECT_EQ(driver.getModulePaths()[1], "vendored");
}

TEST_F(CompilerDriverTest, DefaultBehaviorIsJitMode) {
    const char* argv[] = {"cdd"};
    ASSERT_TRUE(driver.parseArguments(1, argv));
    EXPECT_TRUE(driver.getJitMode());
    EXPECT_TRUE(driver.getInputFiles().empty());
}

TEST_F(CompilerDriverTest, ParseInputFile) {
    const char* argv[] = {"cdd", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(2, argv));
    ASSERT_EQ(driver.getInputFiles().size(), 1u);
    EXPECT_EQ(driver.getInputFiles()[0], "test.cdd");
    EXPECT_FALSE(driver.getJitMode());
}

TEST_F(CompilerDriverTest, ParseMultipleInputFiles) {
    const char* argv[] = {"cdd", "a.cdd", "b.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    ASSERT_EQ(driver.getInputFiles().size(), 2u);
    EXPECT_EQ(driver.getInputFiles()[0], "a.cdd");
    EXPECT_EQ(driver.getInputFiles()[1], "b.cdd");
}

TEST_F(CompilerDriverTest, ParseEmitIR) {
    const char* argv[] = {"cdd", "-S", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getEmitIR());
}

TEST_F(CompilerDriverTest, ParseSyntaxOnly) {
    const char* argv[] = {"cdd", "-fsyntax-only", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getSyntaxOnly());
}

TEST_F(CompilerDriverTest, ParseWall) {
    const char* argv[] = {"cdd", "-Wall", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getWall());
}

TEST_F(CompilerDriverTest, ParseWerror) {
    const char* argv[] = {"cdd", "-Werror", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getWerror());
}

TEST_F(CompilerDriverTest, ParseStdStandard) {
    const char* argv[] = {"cdd", "-std", "c11", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    EXPECT_EQ(driver.getStdStandard(), "c11");
}

TEST_F(CompilerDriverTest, ParseOptLevels) {
    {
        const char* argv[] = {"cdd", "-O", "0", "test.cdd"};
        ASSERT_TRUE(driver.parseArguments(4, argv));
        EXPECT_EQ(driver.getOptLevel(), 0);
    }
    {
        CompilerDriver d;
        const char* argv[] = {"cdd", "-O", "2", "test.cdd"};
        ASSERT_TRUE(d.parseArguments(4, argv));
        EXPECT_EQ(d.getOptLevel(), 2);
    }
    {
        CompilerDriver d;
        const char* argv[] = {"cdd", "-O", "3", "test.cdd"};
        ASSERT_TRUE(d.parseArguments(4, argv));
        EXPECT_EQ(d.getOptLevel(), 3);
    }
}

TEST_F(CompilerDriverTest, ParseDebugFlag) {
    const char* argv[] = {"cdd", "-g", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getIncludeDebug());
}

TEST_F(CompilerDriverTest, ParseVerboseFlag) {
    const char* argv[] = {"cdd", "-v", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getVerbose());
}

TEST_F(CompilerDriverTest, ParseLib) {
    const char* argv[] = {"cdd", "-l", "m", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    ASSERT_EQ(driver.getLibs().size(), 1u);
    EXPECT_EQ(driver.getLibs()[0], "m");
}

TEST_F(CompilerDriverTest, ParseMultipleLibs) {
    const char* argv[] = {"cdd", "-l", "m", "-l", "pthread", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    ASSERT_EQ(driver.getLibs().size(), 2u);
    EXPECT_EQ(driver.getLibs()[0], "m");
    EXPECT_EQ(driver.getLibs()[1], "pthread");
}

TEST_F(CompilerDriverTest, ParseLibPath) {
    const char* argv[] = {"cdd", "-L", "/usr/lib", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    ASSERT_EQ(driver.getLibPaths().size(), 1u);
    EXPECT_EQ(driver.getLibPaths()[0], "/usr/lib");
}

TEST_F(CompilerDriverTest, ParseMultipleLibPaths) {
    const char* argv[] = {"cdd", "-L", "/usr/lib", "-L", "/usr/local/lib", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    ASSERT_EQ(driver.getLibPaths().size(), 2u);
    EXPECT_EQ(driver.getLibPaths()[0], "/usr/lib");
    EXPECT_EQ(driver.getLibPaths()[1], "/usr/local/lib");
}

TEST_F(CompilerDriverTest, ParseCombinedFlags) {
    const char* argv[] = {"cdd", "-c", "-g", "-O", "2", "-M", "libs", "-o", "out.o", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(10, argv));
    EXPECT_TRUE(driver.getCompileOnly());
    EXPECT_TRUE(driver.getIncludeDebug());
    EXPECT_EQ(driver.getOptLevel(), 2);
    ASSERT_EQ(driver.getModulePaths().size(), 1u);
    EXPECT_EQ(driver.getModulePaths()[0], "libs");
    EXPECT_EQ(driver.getOutputFile(), "out.o");
    ASSERT_EQ(driver.getInputFiles().size(), 1u);
    EXPECT_EQ(driver.getInputFiles()[0], "test.cdd");
}

TEST_F(CompilerDriverTest, UnknownOptionFails) {
    const char* argv[] = {"cdd", "--unknown", "test.cdd"};
    EXPECT_FALSE(driver.parseArguments(3, argv));
}

TEST_F(CompilerDriverTest, MissingOutputArgumentFails) {
    const char* argv[] = {"cdd", "-o"};
    EXPECT_FALSE(driver.parseArguments(2, argv));
}

TEST_F(CompilerDriverTest, RunReturnsZero) {
    const char* argv[] = {"cdd"};
    ASSERT_TRUE(driver.parseArguments(1, argv));
    EXPECT_EQ(driver.run(), 0);
}

TEST_F(CompilerDriverTest, RunJitModeReturnsZero) {
    const char* argv[] = {"cdd"};
    ASSERT_TRUE(driver.parseArguments(1, argv));
    EXPECT_EQ(driver.run(), 0);
}

TEST_F(CompilerDriverTest, DefaultOutputIsAOut) {
    const char* argv[] = {"cdd", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(2, argv));
    EXPECT_EQ(driver.getOutputFile(), "a.out");
}

TEST_F(CompilerDriverTest, PrintHelpUsesCDD) {
    testing::internal::CaptureStdout();
    driver.printHelp();
    const std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("Usage: cdd"), std::string::npos);
}

namespace {
std::filesystem::path writeTempSource(const std::string& name, const std::string& text) {
    std::filesystem::path p = std::filesystem::temp_directory_path() / name;
    std::ofstream out(p);
    out << text;
    return p;
}

std::size_t countOccurrences(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

std::string extractFunction(const std::string& ir, const std::string& name) {
    const std::string plainMarker = "@" + name + "(";
    const std::string quotedMarker = "@\"" + name + "\"(";
    std::size_t pos = ir.find("define");
    while (pos != std::string::npos) {
        const std::size_t end = ir.find("\n}", pos);
        if (end == std::string::npos) {
            return {};
        }
        std::string body = ir.substr(pos, end - pos + 2);
        const std::size_t signature_end = body.find('\n');
        if (signature_end != std::string::npos) {
            const std::string signature = body.substr(0, signature_end);
            if (signature.find(plainMarker) != std::string::npos ||
                signature.find(quotedMarker) != std::string::npos) {
                return body;
            }
        }
        pos = ir.find("define", end);
    }
    return {};
}

class ScopedLogSilencer {
public:
    ScopedLogSilencer() : previous(spdlog::default_logger()->level()) {
        spdlog::set_level(spdlog::level::off);
    }
    ~ScopedLogSilencer() {
        spdlog::set_level(previous);
    }

private:
    spdlog::level::level_enum previous;
};

std::string compileToIR(CompilerDriver& driver, const char* argv[], int argc) {
    ScopedLogSilencer silence;
    if (!driver.parseArguments(argc, argv)) {
        return {};
    }
    testing::internal::CaptureStdout();
    if (driver.run() != 0) {
        testing::internal::GetCapturedStdout();
        return {};
    }
    return testing::internal::GetCapturedStdout();
}
} // namespace

TEST_F(CompilerDriverTest, RejectsLegacySmcInput) {
    const std::filesystem::path p =
        writeTempSource("legacy_input.smc", "int32 main() { return 0; }\n");
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-fsyntax-only", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    testing::internal::CaptureStderr();
    EXPECT_EQ(driver.run(), 1);
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("legacy .smc source is not supported"), std::string::npos);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, BareInputSearchesItsDirectory) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_driver_bare_input_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "util.cdd");
        out << "module util;\nexport int32 helper() { return 5; }\n";
    }
    {
        std::ofstream out(dir / "main.cdd");
        out << "module main;\nimport util;\n"
               "int32 main() { return helper(); }\n";
    }
    const std::filesystem::path old = std::filesystem::current_path();
    std::filesystem::current_path(dir);
    const char* argv[] = {"cdd", "-fsyntax-only", "main.cdd"};
    const bool parsed = driver.parseArguments(3, argv);
    const int result = parsed ? driver.run() : -1;
    std::filesystem::current_path(old);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(result, 0);
    std::filesystem::remove_all(dir);
}

TEST_F(CompilerDriverTest, RejectsLegacySmcHiddenFile) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_driver_hidden_smc_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::filesystem::path p = dir / ".smc";
    {
        std::ofstream out(p);
        out << "int32 main() { return 0; }\n";
    }
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-fsyntax-only", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    testing::internal::CaptureStderr();
    EXPECT_EQ(driver.run(), 1);
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("legacy .smc source is not supported"), std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_F(CompilerDriverTest, LexSyntaxOnlySucceedsForValidSource) {
    const std::filesystem::path p =
        writeTempSource("cdd_driver_valid.cdd", "int32 main() { return 0; }\n");
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-fsyntax-only", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_EQ(driver.run(), 0);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, InvalidCharacterFailsCompilation) {
    // LEX-13: the driver must surface lexer diagnostics and fail the build.
    const std::filesystem::path p = writeTempSource("cdd_driver_badlex.cdd", "int32 @ x;\n");
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-fsyntax-only", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_EQ(driver.run(), 1);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, ThreadProgramLinksAndRuns) {
    // std.thread requires the driver to append -l pthread for generated
    // programs; this exercises the real compile+link path, not the JIT.
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_thread_driver_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path src = dir / "thread.cdd";
    const std::filesystem::path out = dir / "thread";
    {
        std::ofstream f(src);
        f << "import std.thread;\n"
             "int64 seen = 0;\n"
             "void* worker(void* arg) { seen = (int64)arg; return nullptr; }\n"
             "int32 main() {\n"
             "    std::Thread t;\n"
             "    t.init();\n"
             "    if (!t.start(worker, (void*)5)) return 1;\n"
             "    if (!t.join()) return 2;\n"
             "    return seen == 5 ? 0 : 3;\n"
             "}\n";
    }
    const std::string srcPath = src.string();
    const std::string outPath = out.string();
    const char* argv[] = {"cdd", "-M", STD_DIR, srcPath.c_str(),
                          "-o", outPath.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    EXPECT_EQ(driver.run(), 0);
    EXPECT_EQ(std::system(outPath.c_str()), 0);
    std::filesystem::remove_all(dir);
}

TEST_F(CompilerDriverTest, MainWithArgsLinksAndRuns) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_main_args_driver_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path src = dir / "main_args.cdd";
    const std::filesystem::path out = dir / "main_args";
    {
        std::ofstream f(src);
        f << "int32 main(int32 argc, char** argv) {\n"
             "    if (argc != 3) return 1;\n"
             "    return argv[1][0] == 'x' ? 0 : 2;\n"
             "}\n";
    }
    const std::string srcPath = src.string();
    const std::string outPath = out.string();
    const char* argv[] = {"cdd", srcPath.c_str(), "-o", outPath.c_str()};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    EXPECT_EQ(driver.run(), 0);
    const std::string command = outPath + " x y";
    EXPECT_EQ(std::system(command.c_str()), 0);
    std::filesystem::remove_all(dir);
}

TEST_F(CompilerDriverTest, PrivateFunctionsInDifferentModulesKeepDistinctSymbols) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_private_module_symbols_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "a.cdd");
        out << "module a;\n"
               "int32 helper() { return 1; }\n"
               "export int32 a_entry() { return helper(); }\n";
    }
    {
        std::ofstream out(dir / "b.cdd");
        out << "module b;\n"
               "int32 helper() { return 2; }\n"
               "export int32 b_entry() { return helper(); }\n";
    }
    {
        std::ofstream out(dir / "main.cdd");
        out << "module main;\n"
               "import a;\n"
               "import b;\n"
               "int32 main() { return a_entry() + b_entry() - 3; }\n";
    }
    const std::string mainPath = (dir / "main.cdd").string();
    const std::string outPath = (dir / "app").string();
    const std::string modulePath = dir.string();
    const char* argv[] = {"cdd", "-M", modulePath.c_str(), mainPath.c_str(),
                          "-o", outPath.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    EXPECT_EQ(driver.run(), 0);
    EXPECT_EQ(std::system(outPath.c_str()), 0);
    std::filesystem::remove_all(dir);
}

TEST_F(CompilerDriverTest, ModuleOperatorOverloadUsesQualifiedSymbol) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_module_operator_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "a.cdd");
        out << "module a;\n"
               "export struct S { int32 v; };\n"
               "export S operator+(S a, S b) { S r; r.v = a.v + b.v; return r; }\n";
    }
    {
        std::ofstream out(dir / "main.cdd");
        out << "module main;\n"
               "import a;\n"
               "int32 main() {\n"
               "    S a; S b; a.v = 2; b.v = 3;\n"
               "    S c = a + b;\n"
               "    return c.v - 5;\n"
               "}\n";
    }
    const std::string mainPath = (dir / "main.cdd").string();
    const std::string outPath = (dir / "app").string();
    const std::string modulePath = dir.string();
    const char* argv[] = {"cdd", "-M", modulePath.c_str(), mainPath.c_str(),
                          "-o", outPath.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    EXPECT_EQ(driver.run(), 0);
    EXPECT_EQ(std::system(outPath.c_str()), 0);
    std::filesystem::remove_all(dir);
}

TEST_F(CompilerDriverTest, PrivateVariablesInDifferentModulesKeepDistinctSymbols) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_private_module_vars_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "a.cdd");
        out << "module a;\n"
               "int32 g = 1;\n"
               "export int32 a_get() { return g; }\n";
    }
    {
        std::ofstream out(dir / "b.cdd");
        out << "module b;\n"
               "int32 g = 2;\n"
               "export int32 b_get() { return g; }\n";
    }
    {
        std::ofstream out(dir / "main.cdd");
        out << "module main;\n"
               "import a;\n"
               "import b;\n"
               "int32 main() { return a_get() + b_get() - 3; }\n";
    }
    const std::string mainPath = (dir / "main.cdd").string();
    const std::string outPath = (dir / "app").string();
    const std::string modulePath = dir.string();
    const char* argv[] = {"cdd", "-M", modulePath.c_str(), mainPath.c_str(),
                          "-o", outPath.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    EXPECT_EQ(driver.run(), 0);
    EXPECT_EQ(std::system(outPath.c_str()), 0);
    std::filesystem::remove_all(dir);
}

TEST_F(CompilerDriverTest, MainWithoutReturnDefaultsToZero) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_main_default_driver_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path src = dir / "main_default.cdd";
    const std::filesystem::path out = dir / "main_default";
    {
        std::ofstream f(src);
        f << "int32 main() { int32 unused = 1; }\n";
    }
    const std::string srcPath = src.string();
    const std::string outPath = out.string();
    const char* argv[] = {"cdd", srcPath.c_str(), "-o", outPath.c_str()};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    EXPECT_EQ(driver.run(), 0);
    EXPECT_EQ(std::system(outPath.c_str()), 0);
    std::filesystem::remove_all(dir);
}

TEST_F(CompilerDriverTest, VoidMainIsRejected) {
    const std::filesystem::path p =
        writeTempSource("cdd_void_main.cdd", "void main() { }\n");
    const std::string path = p.string();
    const std::string outPath = path + ".out";
    const char* argv[] = {"cdd", path.c_str(), "-o", outPath.c_str()};
    ASSERT_TRUE(driver.parseArguments(4, argv));
    testing::internal::CaptureStderr();
    EXPECT_EQ(driver.run(), 1);
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("main"), std::string::npos);
    std::filesystem::remove(p);
    std::filesystem::remove(outPath);
}

TEST_F(CompilerDriverTest, SyntaxOnlyRunsSemanticChecks) {
    const std::filesystem::path p =
        writeTempSource("cdd_syntax_sema.cdd",
                        "int32 main() { int32 x = \"not an int\"; return x; }\n");
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-fsyntax-only", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    testing::internal::CaptureStderr();
    EXPECT_EQ(driver.run(), 1);
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("type mismatch"), std::string::npos);
    std::filesystem::remove(p);
}

// CG-15 regression: loop-local allocas must live in the function entry block;
// otherwise -O0 grows the stack by one frame per iteration and eventually
// segfaults (mem2reg only hides this at -O1+).
TEST_F(CompilerDriverTest, O0LoopLocalsDoNotGrowStack) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("cdd_o0_loop_driver_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path src = dir / "loop.cdd";
    const std::filesystem::path out = dir / "loop";
    {
        std::ofstream f(src);
        f << "int32 main() {\n"
             "    int64 sum = 0;\n"
             "    for (int32 i = 0; i < 1000000; i = i + 1) {\n"
             "        int32 x = i;\n"
             "        sum = sum + (int64)x;\n"
             "    }\n"
             "    return sum == 499999500000 ? 0 : 1;\n"
             "}\n";
    }
    const std::string srcPath = src.string();
    const std::string outPath = out.string();
    const char* argv[] = {"cdd", "-O", "0", srcPath.c_str(), "-o", outPath.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    EXPECT_EQ(driver.run(), 0);
    EXPECT_EQ(std::system(outPath.c_str()), 0);
    std::filesystem::remove_all(dir);
}

// The optimization pipeline must see the target machine: without it the loop
// vectorizer has no SIMD width and bench_matmul stays scalar.
TEST_F(CompilerDriverTest, OptimizedIREnablesGenericVectorization) {
    const std::filesystem::path p = writeTempSource("cdd_simd_generic.cdd", R"(
int32 main() {
    float64 a[128] = {};
    float64 b[128] = {};
    float64 c[128] = {};
    for (int32 i = 0; i < 128; i = i + 1) {
        a[i] = static_cast<float64>(i) * 0.5;
        b[i] = static_cast<float64>(i) * 0.25;
    }
    for (int32 i = 0; i < 128; i = i + 1) {
        c[i] = a[i] + b[i] * 2.0;
    }
    return c[0] == 1.0 ? 0 : 1;
}
)");
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-S", "-O", "2", "-march=generic", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    testing::internal::CaptureStdout();
    EXPECT_EQ(driver.run(), 0);
    const std::string ir = testing::internal::GetCapturedStdout();
    EXPECT_NE(ir.find("llvm.loop.isvectorized"), std::string::npos);
    EXPECT_NE(ir.find("<2 x double>"), std::string::npos);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, ParseMarchNative) {
    const char* argv[] = {"cdd", "-march=native", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_EQ(driver.getMarch(), "native");
}

TEST_F(CompilerDriverTest, NativeMarchVectorizesWithHostFeatures) {
    const std::filesystem::path p = writeTempSource("cdd_simd_native.cdd", R"(
int32 main() {
    float64 a[128] = {};
    float64 b[128] = {};
    float64 c[128] = {};
    for (int32 i = 0; i < 128; i = i + 1) {
        a[i] = static_cast<float64>(i) * 0.5;
        b[i] = static_cast<float64>(i) * 0.25;
    }
    for (int32 i = 0; i < 128; i = i + 1) {
        c[i] = a[i] + b[i] * 2.0;
    }
    return c[0] == 1.0 ? 0 : 1;
}
)");
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-S", "-O", "2", "-march=native", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(6, argv));
    testing::internal::CaptureStdout();
    EXPECT_EQ(driver.run(), 0);
    const std::string ir = testing::internal::GetCapturedStdout();
    EXPECT_NE(ir.find("llvm.loop.isvectorized"), std::string::npos);
    std::filesystem::remove(p);
}

// The default target is host-adaptive: x86-64-v3 on AVX2 hosts, generic
// otherwise. Explicit -march=generic remains the portable escape hatch.
TEST_F(CompilerDriverTest, DefaultMarchIsAuto) {
    const char* argv[] = {"cdd", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(2, argv));
    EXPECT_EQ(driver.getMarch(), "auto");
}

TEST_F(CompilerDriverTest, ParseMarchGeneric) {
    const char* argv[] = {"cdd", "-march=generic", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_EQ(driver.getMarch(), "generic");
}

// O3's non-trivial loop unswitching is a large win on branchy backtracking
// code (nqueens). O2 runs it as a post-pass so the win is available without
// O3's loop-unrolling regressions.
TEST_F(CompilerDriverTest, O2UnswitchesLoopInvariantBranches) {
    const std::filesystem::path p = writeTempSource("cdd_unswitch.cdd", R"(
module cdd_unswitch;
int32 solutions;
bool safe(int32* cols, int32 row, int32 col) {
    for (int32 i = 0; i < row; i = i + 1) {
        int32 c = cols[i];
        if (c == col) { return false; }
        int32 d = row - i;
        if (c - col == d || col - c == d) { return false; }
    }
    return true;
}
void solve(int32* cols, int32 row, int32 n) {
    if (row == n) { solutions = solutions + 1; return; }
    for (int32 c = 0; c < n; c = c + 1) {
        if (safe(cols, row, c)) {
            cols[row] = c;
            solve(cols, row + 1, n);
        }
    }
}
int32 main() {
    int32* cols = (int32*)malloc(16);
    solutions = 0;
    solve(cols, 0, 4);
    println("{}", solutions);
    free(cols);
    return 0;
}
)");
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-S", "-O", "2", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(5, argv));
    testing::internal::CaptureStdout();
    EXPECT_EQ(driver.run(), 0);
    const std::string ir = testing::internal::GetCapturedStdout();
    EXPECT_NE(ir.find("for.body.us"), std::string::npos);
    std::filesystem::remove(p);
}

// Deque rings and open-addressing hash tables both keep a power-of-two
// capacity. Their hot-path indices must lower to masks, not urem/srem.
TEST_F(CompilerDriverTest, StdlibPowerOfTwoIndexingAvoidsDivision) {
    const std::filesystem::path p = writeTempSource("cdd_pow2_index.cdd", R"(
import std.deque;
import std.hashmap;
import std.hash;
int32 main() {
    std::Deque<int32> d = {};
    d.init();
    d.push_back(1);
    d.push_front(0);
    int32 x = 0;
    if (!d.pop_front(&x)) return 1;
    if (!d.pop_back(&x)) return 2;
    d.destroy();

    std::HashMap<uint32,int32> m = {};
    m.init(std::hash_key_u32, std::equal_u32);
    if (!m.put(1, 2)) return 3;
    int32 v = 0;
    if (!m.get(1, &v) || v != 2) return 4;
    if (!m.remove(1)) return 5;
    m.destroy();
    return 0;
}
)");
    const std::string path = p.string();
    const char* argv[] = {"cdd", "-S", "-O", "2", path.c_str()};
    ASSERT_TRUE(driver.parseArguments(5, argv));
    testing::internal::CaptureStdout();
    EXPECT_EQ(driver.run(), 0);
    const std::string ir = testing::internal::GetCapturedStdout();
    EXPECT_NE(ir.find("push__back_"), std::string::npos);
    EXPECT_NE(ir.find("put__raw_"), std::string::npos);
    EXPECT_EQ(ir.find("urem"), std::string::npos);
    EXPECT_EQ(ir.find("srem"), std::string::npos);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, DefaultLinksPie) {
    const char* argv[] = {"cdd", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(2, argv));
    EXPECT_TRUE(driver.getPie());
}

TEST_F(CompilerDriverTest, ParseNoPieFlag) {
    const char* argv[] = {"cdd", "-no-pie", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_FALSE(driver.getPie());
}

TEST_F(CompilerDriverTest, ParsePieFlag) {
    const char* argv[] = {"cdd", "-pie", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_TRUE(driver.getPie());
}

TEST_F(CompilerDriverTest, RecursiveInliningIsOnByDefault) {
    const char* argv[] = {"cdd", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(2, argv));
    EXPECT_TRUE(driver.getRecursiveInlining());
    EXPECT_FALSE(driver.getRecursiveInliningExplicit());
    EXPECT_EQ(driver.getRecursiveInliningDepth(), 4u);
}

TEST_F(CompilerDriverTest, ParseNoRecursiveInlining) {
    const char* argv[] = {"cdd", "-fno-recursive-inlining", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_FALSE(driver.getRecursiveInlining());
    EXPECT_FALSE(driver.getRecursiveInliningExplicit());
}

TEST_F(CompilerDriverTest, ParseRecursiveInliningDepth) {
    const char* argv[] = {"cdd", "-frecursive-inlining-depth=2", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(3, argv));
    EXPECT_EQ(driver.getRecursiveInliningDepth(), 2u);
}

TEST_F(CompilerDriverTest, ParseRecursiveInliningForce) {
    const char* argv[] = {"cdd", "-O", "3", "-frecursive-inlining", "test.cdd"};
    ASSERT_TRUE(driver.parseArguments(5, argv));
    EXPECT_TRUE(driver.getRecursiveInlining());
    EXPECT_TRUE(driver.getRecursiveInliningExplicit());
}

TEST_F(CompilerDriverTest, RejectsMalformedRecursiveInliningDepth) {
    const char* argv[] = {"cdd", "-frecursive-inlining-depth=2x", "test.cdd"};
    EXPECT_FALSE(driver.parseArguments(3, argv));
}

TEST_F(CompilerDriverTest, RecursiveFibGrowsOnlyWhenEnabled) {
    const auto p = writeTempSource("cdd_rec_fib.cdd", R"(
int32 fib(int32 n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
int32 main() { println("{}", fib(10)); return 0; }
)");
    const std::string path = p.string();

    const char* on[] = {"cdd", "-S", "-O", "2", path.c_str()};
    const std::string withRec = compileToIR(driver, on, 5);
    ASSERT_FALSE(withRec.empty());

    const char* off[] = {"cdd", "-S", "-O", "2", "-fno-recursive-inlining",
                         path.c_str()};
    const std::string withoutRec = compileToIR(driver, off, 6);
    ASSERT_FALSE(withoutRec.empty());

    EXPECT_GT(withRec.size(), withoutRec.size());
    const std::string fibBody = extractFunction(withRec, "cdd$f$fib_int32");
    ASSERT_FALSE(fibBody.empty());
    EXPECT_EQ(countOccurrences(fibBody, "call i32 @\"cdd$f$fib_int32\""), 1u);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, RecursiveInliningIsO2OnlyUnlessForced) {
    const auto p = writeTempSource("cdd_rec_o1.cdd", R"(
int32 fib(int32 n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
int32 main() { println("{}", fib(10)); return 0; }
)");
    const std::string path = p.string();

    const char* on[] = {"cdd", "-S", "-O", "1", "-frecursive-inlining",
                        path.c_str()};
    const std::string withFlag = compileToIR(driver, on, 6);

    const char* off[] = {"cdd", "-S", "-O", "1", "-fno-recursive-inlining",
                         path.c_str()};
    const std::string withoutFlag = compileToIR(driver, off, 6);

    ASSERT_FALSE(withFlag.empty());
    ASSERT_FALSE(withoutFlag.empty());
    EXPECT_EQ(withFlag, withoutFlag);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, RecursiveInliningLeavesLeafModulesIdentical) {
    const auto p = writeTempSource("cdd_rec_leaf.cdd", R"(
int32 add_one(int32 n) { return n + 1; }
int32 main() { println("{}", add_one(41)); return 0; }
)");
    const std::string path = p.string();

    const char* on[] = {"cdd", "-S", "-O", "2", path.c_str()};
    const std::string withoutFlag = compileToIR(driver, on, 5);

    const char* off[] = {"cdd", "-S", "-O", "2", "-fno-recursive-inlining",
                         path.c_str()};
    const std::string withOffFlag = compileToIR(driver, off, 6);

    ASSERT_FALSE(withoutFlag.empty());
    ASSERT_FALSE(withOffFlag.empty());
    EXPECT_EQ(withoutFlag, withOffFlag);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, RecursiveInliningIsOffByDefaultAtO3) {
    const auto p = writeTempSource("cdd_rec_o3_default.cdd", R"(
int32 fib(int32 n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
int32 main() { println("{}", fib(10)); return 0; }
)");
    const std::string path = p.string();

    const char* on[] = {"cdd", "-S", "-O", "3", path.c_str()};
    const std::string defaultO3 = compileToIR(driver, on, 5);

    const char* off[] = {"cdd", "-S", "-O", "3", "-fno-recursive-inlining",
                         path.c_str()};
    const std::string disabledO3 = compileToIR(driver, off, 6);

    ASSERT_FALSE(defaultO3.empty());
    ASSERT_FALSE(disabledO3.empty());
    EXPECT_EQ(defaultO3, disabledO3);
    std::filesystem::remove(p);
}

TEST_F(CompilerDriverTest, RecursiveWorkloadsStayUnderPostRerunInstructionCap) {
    struct Case {
        const char *sourceName;
        const char *functionName;
        const char *source;
    };
    const Case cases[] = {
        {"cdd_rec_cap_fib.cdd", "cdd$f$fib_int32", R"(
int32 fib(int32 n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
int32 main() { println("{}", fib(10)); return 0; }
)"},
        {"cdd_rec_cap_nqueens.cdd", "cdd$f$solve_int32ptr_int32_int32", R"(
int32 solutions;
bool safe(int32* cols, int32 row, int32 col) {
    for (int32 i = 0; i < row; i = i + 1) {
        int32 c = cols[i];
        if (c == col) { return false; }
        int32 d = row - i;
        if (c - col == d || col - c == d) { return false; }
    }
    return true;
}
void solve(int32* cols, int32 row, int32 n) {
    if (row == n) { solutions = solutions + 1; return; }
    for (int32 c = 0; c < n; c = c + 1) {
        if (safe(cols, row, c)) { cols[row] = c; solve(cols, row + 1, n); }
    }
}
int32 main() {
    int32* cols = (int32*)malloc(16);
    solutions = 0;
    solve(cols, 0, 4);
    println("{}", solutions);
    free(cols);
    return 0;
}
)"},
    };

    for (const Case &testCase : cases) {
        const auto p = writeTempSource(testCase.sourceName, testCase.source);
        const std::string path = p.string();
        const char *argv[] = {"cdd", "-S", "-O", "2", path.c_str()};
        const std::string ir = compileToIR(driver, argv, 5);
        ASSERT_FALSE(ir.empty()) << testCase.sourceName;

        llvm::LLVMContext context;
        llvm::SMDiagnostic error;
        auto module = llvm::parseAssemblyString(ir, error, context);
        ASSERT_NE(module, nullptr) << testCase.sourceName;
        llvm::Function *function = module->getFunction(testCase.functionName);
        ASSERT_NE(function, nullptr) << testCase.sourceName;
        EXPECT_LE(function->getInstructionCount(), 1000u)
            << testCase.sourceName << " " << testCase.functionName;
        std::filesystem::remove(p);
    }
}

TEST_F(CompilerDriverTest, RecursiveInliningCanBeForcedAtO3) {
    const auto p = writeTempSource("cdd_rec_o3.cdd", R"(
int32 fib(int32 n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
int32 main() { println("{}", fib(10)); return 0; }
)");
    const std::string path = p.string();

    const char* on[] = {"cdd", "-S", "-O", "3", "-frecursive-inlining",
                        path.c_str()};
    const std::string forced = compileToIR(driver, on, 6);

    const char* off[] = {"cdd", "-S", "-O", "3", "-fno-recursive-inlining",
                         path.c_str()};
    const std::string baseline = compileToIR(driver, off, 6);

    ASSERT_FALSE(forced.empty());
    ASSERT_FALSE(baseline.empty());
    EXPECT_GT(forced.size(), baseline.size());
    std::filesystem::remove(p);
}
