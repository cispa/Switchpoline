#ifndef LLVM_NOIC_FINAL_CHECK_PASS_H
#define LLVM_NOIC_FINAL_CHECK_PASS_H

#include <llvm/IR/PassManager.h>
#include <utility>

namespace llvm {

    struct NoICFinalCheckPass : public PassInfoMixin<NoICFinalCheckPass> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
    };

    struct NoICFinalCheckLegacyPass : public ModulePass {
        static char ID;

        NoICFinalCheckLegacyPass();

        bool runOnModule(Module &M) override;

        void getAnalysisUsage(AnalysisUsage &usage) const override;

    };

} // namespace llvm

#endif // LLVM_NOIC_FINAL_CHECK_PASS_H