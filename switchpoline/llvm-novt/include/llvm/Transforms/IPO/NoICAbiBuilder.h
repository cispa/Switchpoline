#ifndef LLVM_NOIC_ABI_BUILDER_H
#define LLVM_NOIC_ABI_BUILDER_H

#include <llvm/IR/PassManager.h>
#include <utility>

namespace llvm {

    struct NoICAbiBuilderPass : public PassInfoMixin<NoICAbiBuilderPass> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
    };

    struct NoICAbiBuilderLegacyPass : public ModulePass {
        static char ID;

        NoICAbiBuilderLegacyPass();

        bool runOnModule(Module &M) override;

        void getAnalysisUsage(AnalysisUsage &usage) const override;

    };

} // namespace llvm

#endif // LLVM_NOIC_ABI_BUILDER_H