#include <set>
#include "llvm/Transforms/IPO/NoICLibraryFixes.h"
#include "llvm/Support/CustomSettings.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

using namespace typegraph;

namespace llvm {

    class NoICLibraryFixesPassInternal {
        Module &M;
        bool IsAfter;
        bool Changed = false;

    public:
        NoICLibraryFixesPassInternal(Module &M, bool IsAfter = false) : M(M), IsAfter(IsAfter) {}

        void GenerateDebugValue(StringRef S, Instruction* Inst) {
          auto *PtrType = Type::getInt8PtrTy(M.getContext());
          auto *Stderr = M.getOrInsertGlobal("stderr", PtrType);
          auto Fprintf = M.getOrInsertFunction("fprintf",
                                               FunctionType::get(Type::getInt32Ty(M.getContext()), {PtrType, PtrType}, true));
          llvm::IRBuilder<> Builder(Inst->getNextNode());
          llvm::errs() << *Fprintf.getCallee() << "\n";
          Builder.CreateCall(Fprintf, {Builder.CreateLoad(Stderr), Builder.CreateGlobalStringPtr(S), Inst});
          llvm::errs() << "[DEBUG] Generated debug value: '" << S << "' for " << *Inst << "\n";
          Changed = true;
        }

        void addGlobalCtor(llvm::Function *F) {
          auto *Void = Type::getVoidTy(M.getContext());
          auto *CharPtr = Type::getInt8PtrTy(M.getContext());
          auto *Int32Ty = Type::getInt32Ty(M.getContext());
          auto *FPT = PointerType::get(FunctionType::get(Void, false), 0);
          llvm::StructType *CtorStructTy = llvm::StructType::get(Int32Ty, FPT, CharPtr);

          auto *Entry =
                  ConstantStruct::get(CtorStructTy, {ConstantInt::get(Int32Ty, 0), F, ConstantPointerNull::get(CharPtr)});
          auto *GV = M.getGlobalVariable("llvm.global_ctors", true);
          if (!GV) {
            auto *AT = ArrayType::get(CtorStructTy, 1);
            GV = cast<GlobalVariable>(M.getOrInsertGlobal("llvm.global_ctors", AT));
            GV->setLinkage(GlobalVariable::AppendingLinkage);
            GV->setInitializer(ConstantArray::get(AT, {Entry}));
          } else {
            // append to existing global_ctors
            auto *OldTy = GV->getType()->getPointerElementType();
            auto *AT = ArrayType::get(CtorStructTy, OldTy->getArrayNumElements() + 1);
            std::vector<Constant *> Init2;
            // some NoIC/NoVT inits might already exist
            if (OldTy->getArrayNumElements() > 0) {
              auto *Init = cast<ConstantArray>(GV->getInitializer());
              for (auto &Op : Init->operands()) {
                Constant *FuncOp = cast<ConstantStruct>(Op)->getOperand(1);
                if (auto *InitFunc = dyn_cast<Function>(FuncOp)) {
                  if (InitFunc->getName().startswith("__no")) {
                    Init2.push_back(cast<Constant>(Op.get()));
                  }
                }
              }
            }
            // Then the new init comes
            Init2.push_back(Entry);
            // Then we add all program/library initializers
            if (OldTy->getArrayNumElements() > 0) {
              auto *Init = cast<ConstantArray>(GV->getInitializer());
              for (auto &Op : Init->operands()) {
                Constant *FuncOp = cast<ConstantStruct>(Op)->getOperand(1);
                auto *InitFunc = dyn_cast<Function>(FuncOp);
                if (!InitFunc || !InitFunc->getName().startswith("__no")) {
                  Init2.push_back(cast<Constant>(Op.get()));
                }
              }
            }
            // Use the new initializer
            GV->setInitializer(nullptr);
            GV->removeFromParent();
            GV = cast<GlobalVariable>(M.getOrInsertGlobal("llvm.global_ctors", AT));
            GV->setLinkage(GlobalVariable::AppendingLinkage);
            GV->setInitializer(ConstantArray::get(AT, Init2));
          }
        }

        void dumpLLVM(StringRef fname) {
          std::error_code code;
          raw_fd_ostream stream(fname, code);
          if (!code) {
            stream << M;
            stream.close();
          }
        }

        bool run() {
          if (!Settings.enabled)
            return false;

          if (!IsAfter && Settings.dump_llvm) {
            dumpLLVM("/tmp/before.ll");
          }

          if (!IsAfter && !Settings.lld_is_shared) {
            // Hard-wire musl libc's call to main(), which is not required to be valid-typed
            hardwire("libc_start_main_stage2", "main");
          }

          if (!IsAfter && Settings.dynamic_linking && !Settings.musl_ldso_mode) {
            hardwire("_dlstart_c", "__dls2");
            hardwire("__dls2", "__dls2b");
            hardwire("__dls2b", "__dls3");
            makePrivate("__dls2");
            makePrivate("__dls2b");
            makePrivate("__dls3");
          }

          if (!IsAfter && Settings.dynamic_linking) {
            addPltRewriter();
          }

          // provide an additional symbol, so that musl libc can resolve __clone's parameter in assembly
          if (!IsAfter && !Settings.musl_ldso_mode) {
            addNoICAssemblyFunctions();
          }

          if (!IsAfter && Settings.musl_ldso_mode) {
            makeEverythingHidden();
          }

          if (IsAfter && Settings.dump_llvm) {
            dumpLLVM("/tmp/after.ll");
          }

          return Changed;
        }

        void addNoICAssemblyFunctions() {// create a dummy function: "__noic_handler_clone"
          auto *PtrType = Type::getInt8PtrTy(M.getContext());
          auto *Int32 = Type::getInt32Ty(M.getContext());
          auto *CallbackType = FunctionType::get(Int32, {PtrType}, false)->getPointerTo();
          auto *HandlerFunction = cast<Function>(
                  M.getOrInsertFunction("__noic_handler_clone", FunctionType::get(Int32, {PtrType, CallbackType}, false)).getCallee());
          if (!HandlerFunction->hasLocalLinkage())
            HandlerFunction->setVisibility(GlobalValue::HiddenVisibility);
          IRBuilder<> Builder(BasicBlock::Create(M.getContext(), "entry", HandlerFunction));
          auto *Call = Builder.CreateCall(HandlerFunction->getArg(1), {HandlerFunction->getArg(0)});
          Builder.CreateRet(Call);
        }

        void addPltRewriter() {
          auto *PltRewriter = M.getFunction("__noic_patch_plt");
          auto *DLS = M.getFunction("__dls2b");
          auto *DLS3 = M.getFunction("__dls3");
          auto *CStart = M.getFunction("_start_c");
          auto *LibCStart = M.getFunction("__libc_start_main");
          auto *DoInit = M.getFunction("__do_init");
          if (PltRewriter) {
            if (DLS) {
              IRBuilder<> Builder(DLS->getEntryBlock().getFirstNonPHI());
              Builder.CreateCall(PltRewriter);
              // llvm::errs() << "[DYNLINK] Patched " << DLS->getName() << "\n";
            } else if (DLS3) {
              IRBuilder<> Builder(DLS3->getEntryBlock().getFirstNonPHI());
              Builder.CreateCall(PltRewriter);
              // llvm::errs() << "[DYNLINK] Patched " << DLS->getName() << "\n";
            } else if (LibCStart && !LibCStart->isDeclaration()) {
              IRBuilder<> Builder(LibCStart->getEntryBlock().getFirstNonPHI());
              Builder.CreateCall(PltRewriter);
              // llvm::errs() << "[DYNLINK] Patched " << LibCStart->getName() << "\n";
            } else if (CStart) {
              IRBuilder<> Builder(CStart->getEntryBlock().getFirstNonPHI());
              Builder.CreateCall(PltRewriter);
              // llvm::errs() << "[DYNLINK] Patched " << CStart->getName() << "\n";
            } else {
              addGlobalCtor(PltRewriter);
              // llvm::errs() << "[DYNLINK] Added global ctor " << "\n";
            }
          } else {
            // llvm::errs() << "[DYNLINK] No rewriter present\n";
            Settings.dynamic_linking = false;
          }
        }

        void hardwire(const std::string &CallingFunction, const std::string &Callee) {
          auto *F = M.getFunction(CallingFunction);
          auto *CalleeFunc = M.getFunction(Callee);
          if (!F || !CalleeFunc)
            return;
          for (auto &BB: *F) {
            for (auto &Ins: BB) {
              if (auto *C = dyn_cast<CallInst>(&Ins)) {
                if (C->isIndirectCall()) {

                  auto *CallType = C->getCalledValue()->getType();
                  if (CalleeFunc->getType() == CallType) {
                    C->setCalledFunction(CalleeFunc);
                  } else {
                    C->setCalledOperand(ConstantExpr::getBitCast(CalleeFunc, CallType));
                  }
                  Changed = true;
                }
              }
            }
          }
        }

        void makePrivate(const std::string FuncName) {
          auto *F = M.getFunction(FuncName);
          if (F) {
            //F->setLinkage(Function::PrivateLinkage);
            F->setVisibility(Function::HiddenVisibility);
          }
        }

        void makeEverythingHidden() {
          for (auto &GV: M.getGlobalList()) {
            if (GV.hasName() && GV.getName() == "min_library_address")
              continue;
            if (GV.getLinkage() == llvm::GlobalValue::CommonLinkage)
              GV.setLinkage(llvm::GlobalValue::PrivateLinkage);
            if (GV.getVisibility() == llvm::GlobalValue::DefaultVisibility && !GV.hasLocalLinkage())
              GV.setVisibility(llvm::GlobalValue::HiddenVisibility);
          }
          std::set<std::string> ExpectedFunctionNames{"_dlstart_c", "__dls2", "__dls2b", "__dls3"};
          for (auto &GV: M.getFunctionList()) {
            if (GV.hasName() && ExpectedFunctionNames.find(GV.getName()) != ExpectedFunctionNames.end())
              continue;
            if (GV.getLinkage() == llvm::GlobalValue::CommonLinkage)
              GV.setLinkage(llvm::GlobalValue::PrivateLinkage);
            if (GV.getVisibility() == llvm::GlobalValue::DefaultVisibility && !GV.hasLocalLinkage())
              GV.setVisibility(llvm::GlobalValue::HiddenVisibility);
          }
        }
    };

    PreservedAnalyses NoICLibraryFixesPass::run(Module &M, ModuleAnalysisManager &MAM) {
      return NoICLibraryFixesPassInternal(M).run() ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    bool NoICLibraryFixesLegacyPass::runOnModule(Module &M) { return NoICLibraryFixesPassInternal(M, isAfter).run(); }

    char NoICLibraryFixesLegacyPass::ID = 0;

    NoICLibraryFixesLegacyPass::NoICLibraryFixesLegacyPass() : ModulePass(ID), isAfter(false) {}
    NoICLibraryFixesLegacyPass::NoICLibraryFixesLegacyPass(bool IsAfter) : ModulePass(ID), isAfter(IsAfter) {}

    void NoICLibraryFixesLegacyPass::getAnalysisUsage(AnalysisUsage &AU) const {}

    static RegisterPass<NoICLibraryFixesLegacyPass> Registration("NoICLibraryFixes", "NoICLibraryFixesLegacyPass", false, false);

} // namespace llvm