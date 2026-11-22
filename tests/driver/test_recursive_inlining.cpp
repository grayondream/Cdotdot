#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "driver/RecursiveInlining.h"

#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

namespace {

unsigned countSelfCalls(const llvm::Function &function) {
    unsigned count = 0;
    for (const llvm::BasicBlock &block : function) {
        for (const llvm::Instruction &instruction : block) {
            if (const auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction)) {
                if (call->getCalledFunction() == &function) {
                    ++count;
                }
            }
        }
    }
    return count;
}

class RecursiveInliningTest : public ::testing::Test {
protected:
    std::unique_ptr<llvm::Module> parse(llvm::StringRef ir) {
        llvm::SMDiagnostic error;
        auto module = llvm::parseAssemblyString(ir, error, context);
        if (!module) {
            error.print("RecursiveInliningTest", llvm::errs());
        }
        return module;
    }

    llvm::LLVMContext context;
};

const char *kSingleSelfCall = R"(
    define i32 @rec(i32 %n) {
    entry:
      %base = icmp eq i32 %n, 0
      br i1 %base, label %done, label %step
    step:
      %m = add i32 %n, -1
      %r = call i32 @rec(i32 %m)
      %v = add i32 %r, 1
      ret i32 %v
    done:
      ret i32 0
    }
)";

const char *kTwoSelfCalls = R"(
    define i32 @fib(i32 %n) {
    entry:
      %base = icmp slt i32 %n, 2
      br i1 %base, label %done, label %rec
    rec:
      %n1 = sub i32 %n, 1
      %a = call i32 @fib(i32 %n1)
      %n2 = sub i32 %n, 2
      %b = call i32 @fib(i32 %n2)
      %v = add i32 %a, %b
      ret i32 %v
    done:
      ret i32 %n
    }
)";

TEST_F(RecursiveInliningTest, SingleSelfCallGrowsButStaysRecursive) {
    auto module = parse(kSingleSelfCall);
    ASSERT_NE(module, nullptr);
    llvm::Function *function = module->getFunction("rec");
    ASSERT_NE(function, nullptr);
    const unsigned initialInstructions = function->getInstructionCount();

    RecursiveInliningOptions options;
    const RecursiveInliningResult result = inlineRecursiveCalls(*module, options);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.modifiedFunctions.size(), 1u);
    EXPECT_TRUE(result.modifiedFunctions.contains(function));
    EXPECT_EQ(countSelfCalls(*function), 1u);
    EXPECT_GT(function->getInstructionCount(), initialInstructions);
    EXPECT_FALSE(llvm::verifyModule(*module, nullptr));
}

TEST_F(RecursiveInliningTest, TwoSelfCallsAreSkippedUnlessAllowed) {
    auto module = parse(kTwoSelfCalls);
    ASSERT_NE(module, nullptr);
    llvm::Function *fib = module->getFunction("fib");
    ASSERT_NE(fib, nullptr);

    RecursiveInliningOptions options;
    const RecursiveInliningResult skipped = inlineRecursiveCalls(*module, options);

    EXPECT_FALSE(skipped.changed);
    EXPECT_TRUE(skipped.modifiedFunctions.empty());

    llvm::Function *allowed[] = {fib};
    const RecursiveInliningResult forced =
        inlineRecursiveCalls(*module, options, allowed);

    EXPECT_TRUE(forced.changed);
    EXPECT_TRUE(forced.modifiedFunctions.contains(fib));
    EXPECT_GT(countSelfCalls(*fib), 0u);
    EXPECT_FALSE(llvm::verifyModule(*module, nullptr));
}

TEST_F(RecursiveInliningTest, InitialSizeFilterIsBypassedByAlsoAllow) {
    auto module = parse(kSingleSelfCall);
    ASSERT_NE(module, nullptr);
    llvm::Function *function = module->getFunction("rec");
    ASSERT_NE(function, nullptr);

    RecursiveInliningOptions options;
    options.maxInitialInstructions = 1;

    const RecursiveInliningResult skipped = inlineRecursiveCalls(*module, options);
    EXPECT_FALSE(skipped.changed);

    llvm::Function *allowed[] = {function};
    const RecursiveInliningResult forced =
        inlineRecursiveCalls(*module, options, allowed);
    EXPECT_TRUE(forced.changed);
}

TEST_F(RecursiveInliningTest, MaxFinalInstructionsStopsBeforeOverflow) {
    auto module = parse(kSingleSelfCall);
    ASSERT_NE(module, nullptr);
    llvm::Function *function = module->getFunction("rec");
    ASSERT_NE(function, nullptr);

    RecursiveInliningOptions options;
    options.maxFinalInstructions = 20;
    const RecursiveInliningResult result = inlineRecursiveCalls(*module, options);

    EXPECT_TRUE(result.changed);
    EXPECT_LE(function->getInstructionCount(), options.maxFinalInstructions);
    EXPECT_FALSE(llvm::verifyModule(*module, nullptr));
}

TEST_F(RecursiveInliningTest, NoSelfCallIsUnchanged) {
    auto module = parse(R"(
        define i32 @leaf(i32 %n) {
        entry:
          %v = add i32 %n, 1
          ret i32 %v
        }
    )");
    ASSERT_NE(module, nullptr);
    llvm::Function *function = module->getFunction("leaf");
    ASSERT_NE(function, nullptr);
    const unsigned initialInstructions = function->getInstructionCount();

    RecursiveInliningOptions options;
    const RecursiveInliningResult result = inlineRecursiveCalls(*module, options);

    EXPECT_FALSE(result.changed);
    EXPECT_TRUE(result.modifiedFunctions.empty());
    EXPECT_EQ(function->getInstructionCount(), initialInstructions);
    EXPECT_FALSE(llvm::verifyModule(*module, nullptr));
}

} // namespace
