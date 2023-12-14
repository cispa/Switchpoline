#include "llvm/Transforms/IPO/NoICFinalCheck.h"
#include "llvm/Support/CustomSettings.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

using namespace typegraph;

namespace llvm {

    class NoICCheckPassInternal {
        Module &M;
        std::vector<llvm::Function *> ReferencedFunctions;

    public:
        NoICCheckPassInternal(Module &M) : M(M) {}

        void dumpLLVM(StringRef fname) {
          std::error_code code;
          raw_fd_ostream stream(fname, code);
          if (!code) {
            stream << M;
            stream.close();
          }
        }

        bool run() {
          if (!Settings.indirect_call_remaining_warn && !Settings.indirect_call_remaining_error)
            return false;

          // Check for indirect calls
          for (auto &F: M.functions()) {
            for (auto &BB: F.getBasicBlockList()) {
              for (auto &Ins: BB) {
                if (auto *C = dyn_cast<CallBase>(&Ins)) {
                  if (C->isIndirectCall()) {
                    if (Settings.indirect_call_remaining_warn) {
                      llvm::errs() << "[WARN] Indirect call remained in function " << (F.hasName() ? F.getName() : "") << ":\n";
                      llvm::errs() << "       " << *C << "\n";
                    }
                    if (Settings.indirect_call_remaining_error) {
                      if (F.getName() == "libc_start_init" || F.getName() == "libc_exit_fini") {
                        llvm::errs() << "         (ignoring this libc error location for now)\n";
                      } else {
                        llvm::report_fatal_error("Indirect call remained, aborting.", false);
                      }
                    }
                  }
                }
              }
            }
          }

          if (Settings.dump_llvm) {
            dumpLLVM("/tmp/after-opt.ll");
          }

          return false;
        }
    };

    PreservedAnalyses NoICFinalCheckPass::run(Module &M, ModuleAnalysisManager &MAM) {
      return NoICCheckPassInternal(M).run() ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    bool NoICFinalCheckLegacyPass::runOnModule(Module &M) { return NoICCheckPassInternal(M).run(); }

    char NoICFinalCheckLegacyPass::ID = 0;

    NoICFinalCheckLegacyPass::NoICFinalCheckLegacyPass() : ModulePass(ID) {}

    void NoICFinalCheckLegacyPass::getAnalysisUsage(AnalysisUsage &AU) const {}

    static RegisterPass<NoICFinalCheckLegacyPass> Registration("NoICFinalCheck", "NoICFinalCheckLegacyPass", false, false);

} // namespace llvm