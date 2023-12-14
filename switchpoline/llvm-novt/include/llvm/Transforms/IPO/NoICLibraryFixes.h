#ifndef LLVM_NOIC_LIBRARYFIXES_PASS_H
#define LLVM_NOIC_LIBRARYFIXES_PASS_H

#include <llvm/IR/PassManager.h>
#include <utility>

namespace llvm {

    struct NoICLibraryFixesPass : public PassInfoMixin<NoICLibraryFixesPass> {
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
    };

    struct NoICLibraryFixesLegacyPass : public ModulePass {
        static char ID;
        bool isAfter;

        NoICLibraryFixesLegacyPass();
        NoICLibraryFixesLegacyPass(bool isAfter);

        bool runOnModule(Module &M) override;

        void getAnalysisUsage(AnalysisUsage &usage) const override;

    };

} // namespace llvm

#endif // LLVM_NOIC_LIBRARYFIXES_PASS_H