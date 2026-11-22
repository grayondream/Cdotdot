#include "RecursiveInlining.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <vector>

namespace {

unsigned countDirectSelfCalls(const llvm::Function &function) {
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

llvm::CallBase *findFirstDirectSelfCall(llvm::Function &function) {
    for (llvm::BasicBlock &block : function) {
        for (llvm::Instruction &instruction : block) {
            auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction);
            if (call != nullptr && call->getCalledFunction() == &function) {
                return call;
            }
        }
    }
    return nullptr;
}

} // namespace

RecursiveInliningResult inlineRecursiveCalls(
    llvm::Module &module,
    const RecursiveInliningOptions &options,
    llvm::ArrayRef<llvm::Function *> alsoAllow) {
    RecursiveInliningResult result;

    llvm::SmallPtrSet<llvm::Function *, 8> allowed;
    for (llvm::Function *function : alsoAllow) {
        if (function != nullptr) {
            allowed.insert(function);
        }
    }

    std::vector<llvm::Function *> targets;
    for (llvm::Function &function : module) {
        if (function.isDeclaration()) {
            continue;
        }
        if (allowed.contains(&function)) {
            targets.push_back(&function);
            continue;
        }
        if (countDirectSelfCalls(function) != 1) {
            continue;
        }
        if (function.getInstructionCount() > options.maxInitialInstructions) {
            continue;
        }
        targets.push_back(&function);
    }

    for (llvm::Function *function : targets) {
        unsigned depth = 0;
        while (depth < options.maxDepth) {
            // Self-inlining duplicates the body. Refuse the inline before it
            // can grow the function beyond the hard cap.
            if (function->getInstructionCount() * 2 > options.maxFinalInstructions) {
                break;
            }

            llvm::CallBase *call = findFirstDirectSelfCall(*function);
            if (call == nullptr) {
                break;
            }

            llvm::InlineFunctionInfo inlineInfo;
            const llvm::InlineResult inlineResult =
                llvm::InlineFunction(*call, inlineInfo);
            if (!inlineResult.isSuccess()) {
                break;
            }

            result.changed = true;
            result.modifiedFunctions.insert(function);
            ++depth;
        }
    }

    return result;
}
