#pragma once

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Function.h"

namespace llvm {
class Module;
}

struct RecursiveInliningOptions {
    unsigned maxDepth = 4;
    unsigned maxInitialInstructions = 100;
    unsigned maxFinalInstructions = 1000;
};

struct RecursiveInliningResult {
    bool changed = false;
    llvm::SmallPtrSet<llvm::Function *, 8> modifiedFunctions;
};

/// Inline direct self-calls one call site at a time, bounded by per-call depth
/// and a hard final instruction cap.
///
/// A function is selected when it has exactly one direct self-call and its
/// initial instruction count is within maxInitialInstructions. Functions listed
/// in alsoAllow bypass both filters, which lets the post-O2 phase continue a
/// function selected by the pre-O2 phase.
RecursiveInliningResult inlineRecursiveCalls(
    llvm::Module &module,
    const RecursiveInliningOptions &options,
    llvm::ArrayRef<llvm::Function *> alsoAllow = {});
