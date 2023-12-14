#include "llvm/Transforms/IPO/NoICAbiBuilder.h"
#include "NoICDispatcherBuilder.h"
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/IRBuilder.h>
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CustomSettings.h"
#include <llvm/Support/JSON.h>
#include <chrono>

using namespace typegraph;

namespace llvm {

    namespace {
        Value *castTo(Value *V, Type *T, IRBuilder<> &B) {
          if (V->getType() != T) {
            return B.CreateBitCast(V, T);
          }
          return V;
        }

        bool isUsedAsCallArgumentOnly(const User *Val) {
          for (const Use &U : Val->uses()) {
            const User *FU = U.getUser();
            if (isa<BlockAddress>(FU))
              continue;
            if (const auto *Call = dyn_cast<CallBase>(FU)) {
              if (!Call->isCallee(&U)) {
                return false;
              }
              continue;
            }
            if (const auto *BC = dyn_cast<BitCastOperator>(FU)) {
              if (!isUsedAsCallArgumentOnly(BC)) {
                return false;
              }
              continue;
            }
            if (const auto *PIC = dyn_cast<PtrToIntOperator>(FU)) {
              if (!isUsedAsCallArgumentOnly(PIC)) {
                return false;
              }
              continue;
            }
            if (const auto *A = dyn_cast<GlobalAlias>(FU)) {
              if (!isUsedAsCallArgumentOnly(A)) {
                return false;
              }
              continue;
            }
            // ignore entries in vtables. vtables might still be around for RTTI
            if (const auto *GV = dyn_cast<GlobalVariable>(FU)) {
              if (GV->getInitializer() != Val)
                return false;
              if (typegraph::Settings.novt_enabled && GV->hasName() && GV->getName().startswith("_ZTV"))
                continue;
              return false;
            }
            // to ignore vtables, we have to look into constants of the form { [...] }
            if (const auto *CA = dyn_cast<ConstantArray>(FU)) {
              if (!isUsedAsCallArgumentOnly(CA))
                return false;
              continue;
            }
            if (const auto *CS = dyn_cast<ConstantStruct>(FU)) {
              if (!isUsedAsCallArgumentOnly(CS))
                return false;
              continue;
            }
            return false;
          }
          return true;
        }
    } // namespace

    class NoICPassInternal {
        Module &M;
        std::vector<llvm::Function *> ReferencedFunctions;

    public:
        NoICPassInternal(Module &M) : M(M) {
        }

        ~NoICPassInternal() {
        }

        bool isCompatible(const llvm::CallBase *Call, const llvm::Function *Function) {
          // return isArgumentCountCompatible(Call, Function);
          return isArgumentTypeCompatible(Call, Function);
        }

        bool isArgumentCountCompatible(const llvm::CallBase *Call, const llvm::Function *Function) {
          auto CallSize = Call->getNumArgOperands();
          auto IsVarArg = Function->isVarArg() || (Call->getCalledValue()->getType()->isFunctionTy() && Call->getCalledValue()->getType()->isFunctionVarArg());
          if (!(Function->arg_size() == CallSize || (IsVarArg && Function->arg_size() < CallSize)))
            return false;
          return true;
        }

        bool isArgumentTypeCompatible(const llvm::CallBase *Call, const llvm::Function *Function) {
          auto CallSize = Call->getNumArgOperands();
          auto IsVarArg = Function->isVarArg() || (Call->getCalledValue()->getType()->isFunctionTy() && Call->getCalledValue()->getType()->isFunctionVarArg());
          if (!(Function->arg_size() == CallSize || (IsVarArg && Function->arg_size() < CallSize)))
            return false;
          for (size_t I = 0; I < Call->arg_size() && I < Function->arg_size(); I++) {
            auto *T1 = Call->getArgOperand(I)->getType();
            auto *T2 = Function->getArg(I)->getType();
            if (T1->isPointerTy() && T2->isPointerTy()) continue;
            if (T1->isIntegerTy() && T2->isIntegerTy()) continue;
            if (T1->isFloatingPointTy() && T2->isFloatingPointTy()) continue;
            if ((T1->isFloatingPointTy() && !T2->isFloatingPointTy()) ||
                (!T1->isFloatingPointTy() && T2->isFloatingPointTy()))
              return false;
            auto S1 = M.getDataLayout().getTypeSizeInBits(T1);
            auto S2 = M.getDataLayout().getTypeSizeInBits(T2);
            if (S1 == S2) continue;
            return false;
          }

          // Check return types
          if (!Call->getType()->isVoidTy()) {
            auto *T1 = Call->getType();
            auto *T2 = Function->getFunctionType()->getReturnType();
            if (T2->isVoidTy()) return false;
            if (T1->isIntegerTy() && T2->isIntegerTy()) return true;
            if (T1->isFloatingPointTy() && T2->isFloatingPointTy()) return true;
            if ((T1->isFloatingPointTy() && !T2->isFloatingPointTy()) ||
                (!T1->isFloatingPointTy() && T2->isFloatingPointTy()))
              return false;
            if (T1->isPointerTy() && T2->isPointerTy()) return true;
            auto S1 = M.getDataLayout().getTypeSizeInBits(T1);
            auto S2 = M.getDataLayout().getTypeSizeInBits(T2);
            if (S1 != S2) return false;
          }

          return true;
        }

        bool isArgumentTypeCompatible(const llvm::FunctionType *FT, const llvm::Function *Function) {
          auto CallSize = FT->getNumParams();
          auto IsVarArg = Function->isVarArg() || FT->isFunctionVarArg();
          if (!(Function->arg_size() == CallSize || (IsVarArg && Function->arg_size() < CallSize)))
            return false;
          for (size_t I = 0; I < FT->getNumParams() && I < Function->arg_size(); I++) {
            auto *T1 = FT->getParamType(I);
            auto *T2 = Function->getArg(I)->getType();
            if (T1->isPointerTy() && T2->isPointerTy()) continue;
            if (T1->isIntegerTy() && T2->isIntegerTy()) continue;
            if (T1->isFloatingPointTy() && T2->isFloatingPointTy()) continue;
            if ((T1->isFloatingPointTy() && !T2->isFloatingPointTy()) ||
                (!T1->isFloatingPointTy() && T2->isFloatingPointTy()))
              return false;
            auto S1 = M.getDataLayout().getTypeSizeInBits(T1);
            auto S2 = M.getDataLayout().getTypeSizeInBits(T2);
            if (S1 == S2) continue;
            return false;
          }

          // Check return types
          if (!FT->getReturnType()->isVoidTy()) {
            auto *T1 = FT->getReturnType();
            auto *T2 = Function->getFunctionType()->getReturnType();
            if (T2->isVoidTy()) return false;
            if (T1->isIntegerTy() && T2->isIntegerTy()) return true;
            if (T1->isFloatingPointTy() && T2->isFloatingPointTy()) return true;
            if ((T1->isFloatingPointTy() && !T2->isFloatingPointTy()) ||
                (!T1->isFloatingPointTy() && T2->isFloatingPointTy()))
              return false;
            if (T1->isPointerTy() && T2->isPointerTy()) return true;
            auto S1 = M.getDataLayout().getTypeSizeInBits(T1);
            auto S2 = M.getDataLayout().getTypeSizeInBits(T2);
            if (S1 != S2) return false;
          }

          return true;
        }

        void collectFunctions() {
          for (auto &F: M.getFunctionList()) {
            if (F.hasAddressTaken()) {
              // exclude our own functions
              if (F.hasName() && F.getName() == "__noic_patch_plt")
                continue;
              // check if it's really taken, of if it's a leftover from vtables
              if (!isUsedAsCallArgumentOnly(&F)) {
                ReferencedFunctions.push_back(&F);
              }
            }
          }
        }

        void addEnforcement() {
          LLVMDispatcherBuilder Builder(M.getContext(), M);

          bool HasMuslDynamicJit = Settings.lld_is_shared && Settings.dynamic_linking && Settings.enforce_jit;
          if (HasMuslDynamicJit) {
            size_t ModuleID = std::hash<std::string>{}(Settings.output_filename);
            ModuleID &= 0x7fffffffffffff00;
            ModuleID &= (1uL << Settings.enforce_id_bitwidth) - 1;
            if (!ModuleID)
              ModuleID = 0xff00;
            fprintf(stderr, "Module ID: %lu (=0x%lx)\n", ModuleID, ModuleID);
            Builder.setModuleID(ModuleID);
          }

          // Collect all indirect calls
          for (auto &F : M.functions()) {
            if (!HasMuslDynamicJit && F.hasName() && (F.getName() == "_dlstart_c" || F.getName() == "__dls2" || F.getName() == "__dls2b"))
              continue;
            // Some musl functions need special threatment for proper application startup
            bool FunctionIsMuslDynamic = F.hasName() && (F.getName() == "do_init_fini" || F.getName() == "libc_start_main_stage2" || F.getName() == "__libc_exit_fini" || F.getName() == "__funcs_on_exit");
            if (Settings.lld_is_shared && !HasMuslDynamicJit && FunctionIsMuslDynamic)
              continue;
            if ((!Settings.link_with_compilerrt || !Settings.link_with_libc || Settings.lld_is_shared) && (F.getName() == "libc_start_init" || F.getName() == "libc_exit_fini"))
              continue;

            for (auto &Bb : F) {
              for (auto &Ins : Bb) {
                if (auto *Call = dyn_cast<CallBase>(&Ins)) {
                  if (Call->isIndirectCall()) {
                    std::string CallName = std::string("<unnamed call> in ") + F.getName().str();
                    auto *MD = Call->getMetadata(LLVMContext::MD_call_name);
                    if (MD && MD->getNumOperands() == 1) {
                      if (auto *MDS = dyn_cast<MDString>(MD->getOperand(0))) {
                        CallName = MDS->getString().str();
                      }
                    }
                    auto &C = Builder.addCall(new std::string(CallName), Call);
                    C.IsExternal = Settings.enforce_jit;

                    // some musl functions don't get ordinary targets, but only target the JIT
                    if (HasMuslDynamicJit && FunctionIsMuslDynamic)
                      continue;

                    for (auto *RF: ReferencedFunctions) {
                      if (!isCompatible(Call, RF)) continue;
                      // if ((F.getName() == "call" || F.getName() == "do_bench") && !RF->getName().startswith("__micro_benchmark")) continue;
                      // Some functions from MUSL are never targets
                      if (RF->getName() == "__restore" || RF->getName() == "__restore_rt" || RF->getName() == "__call_function_ptr_handler")
                        continue;
                      auto *Func = Builder.getFunction(RF);
                      Func->Leaking = Settings.dynamic_linking;
                      if (std::find(C.Targets.begin(), C.Targets.end(), Func) == C.Targets.end()) {
                        C.Targets.push_back(Func);
                      }
                    }

                  } else if (Call->getCalledFunction() && Call->getCalledFunction()->hasName() && Call->getCalledFunction()->getName().startswith("__noic_resolve_")) {
                    // resolve points
                    auto *RPName = new std::string((std::string("RP ") + Call->getCalledFunction()->getName()).str());
                    auto &C = Builder.addResolveFunction(RPName, Call);
                    auto *FT = cast<FunctionType>(cast<PointerType>(Call->getArgOperand(0)->getType())->getPointerElementType());

                    for (auto *RF: ReferencedFunctions) {
                      if (!isArgumentTypeCompatible(FT, RF)) continue;
                      if (RF->getName() == "__restore" || RF->getName() == "__restore_rt" || RF->getName() == "__call_function_ptr_handler")
                        continue;
                      auto *Func = Builder.getFunction(RF);
                      Func->Leaking = Settings.dynamic_linking;
                      if (std::find(C.Targets.begin(), C.Targets.end(), Func) == C.Targets.end()) {
                        C.Targets.push_back(Func);
                      }
                    }
                  }
                }
              }
            }
          }

          // build a special function to send to the JIT later
          if ((Settings.enforce_jit && Settings.dynamic_linking) || Settings.noic_all_id_func) {
            auto *Int64 = IntegerType::getInt64Ty(M.getContext());
            auto *FType = FunctionType::get(Int64, {Int64, Int64, Int64, Int64, Int64, Int64, Int64, Int64}, false);
            auto *F = cast<Function>(M.getOrInsertFunction("__noic_all_id_handler", FType).getCallee());
            auto *BB = BasicBlock::Create(M.getContext(), "entry", F);
            IRBuilder<> IRB(BB);
            std::vector<llvm::Value *> Args;
            for (auto &A: F->args())
              Args.push_back(&A);
            std::string Constraints;
            auto TT = StringRef(M.getTargetTriple());
            if (TT.startswith("x86_64-")) {
              Constraints = "={r11},~{dirflag},~{fpsr},~{flags}";
            } else if (TT.startswith("aarch64-")) {
              Constraints = "={x13}";
            } else {
              llvm::report_fatal_error("Unsupported target architecture for dynamic linking!");
            }
            auto *Asm = InlineAsm::get(FunctionType::get(FType->getPointerTo(), {}, false),"", Constraints, true);
            auto *FPtr = IRB.CreateCall(Asm);
            auto *Call = IRB.CreateCall(FPtr, Args);
            IRB.CreateRet(Call);
            // all known functions are possible targets
            auto &C = Builder.addCall(new std::string("call in __noic_all_id_handler"), Call);
            C.IsExternal = false;
            for (const auto &It: Builder.getFunctions()) {
              if (std::find(C.Targets.begin(), C.Targets.end(), It.second.get()) == C.Targets.end()) {
                C.Targets.push_back(It.second.get());
              }
            }
          }

          // Build enforcement
          long MinID = Builder.getNextId();
          Builder.initialize();
          Builder.assignOptimalIDs();
          Builder.assignMissingIDs();
          Builder.debugIDAssignment();
          Builder.replaceFunctionsWithIDs();
          Builder.generateCodeForAll();

          Builder.printFunctionIDs();
          Builder.writeFunctionIDsToFile();
          if (Settings.export_noic) {
            Builder.writeStatistics("noic-output.txt");
          }
          // llvm::outs() << M << "\n\n\n";

          // Register __noic_all_id_handler with JIT
          if ((Settings.enforce_jit && Settings.dynamic_linking && Builder.getNextId() > MinID) || Settings.noic_all_id_func) {
            auto *HandlerFunc = M.getFunction("__noic_all_id_handler");
            for (auto &BB: *HandlerFunc) {
              for (auto &Ins: BB) {
                if (auto *C = dyn_cast<CallInst>(&Ins)) {
                  C->setTailCall(true);
                  //C->setTailCallKind(CallInst::TCK_MustTail);
                }
              }
            }

            auto *VoidTy = Type::getVoidTy(M.getContext());
            auto *PtrTy = Type::getInt8PtrTy(M.getContext());
            auto *IntTy = Type::getInt64Ty(M.getContext());
            auto *RegisterFunc = cast<Function>(M.getOrInsertFunction("__noic_all_id_handler_register", FunctionType::get(VoidTy, {}, false)).getCallee());
            auto JITRegisterFunc = M.getOrInsertFunction("__noic_register_handler", FunctionType::get(VoidTy, {PtrTy, IntTy, IntTy}, false));
            // registerfunc calls JITRegisterFunc(HandlerFunc, MinId, MaxId)
            auto *BB = BasicBlock::Create(M.getContext(), "entry", RegisterFunc);
            IRBuilder<> IRB(BB);
            IRB.CreateCall(JITRegisterFunc, {IRB.CreateBitCast(HandlerFunc, PtrTy), ConstantInt::get(IntTy, MinID), ConstantInt::get(IntTy, Builder.getNextId()-1)});
            IRB.CreateRetVoid();
            // call registerfunc on startup
            RegisterFunc->setLinkage(Function::InternalLinkage);
            HandlerFunc->setLinkage(Function::InternalLinkage);
            addGlobalCtor(RegisterFunc);
          }
        }

        void reportSettings() {
          /*llvm::errs() << "[Setting] TG_INSTRUMENT_COLLECTCALLTARGETS = " << Settings.instrument_collectcalltargets << "\n";
          llvm::errs() << "[Setting] TG_LINKTIME_LAYERING = " << Settings.linktime_layering << "\n";
          llvm::errs() << "[Setting] TG_ENFORCE = " << Settings.enforce << "\n";
          llvm::errs() << "[Setting] TG_DYNLIB_SUPPORT = " << Settings.dynlib_support << "\n";
          llvm::errs() << "[Setting] TG_ENFORCE_ID_BITWIDTH = " << Settings.enforce_id_bitwidth << "\n";
          llvm::errs() << "[Setting] TG_ENFORCE_DISPATCHER_LIMIT = " << Settings.enforce_dispatcher_limit << "\n";
          llvm::errs() << "[Setting] TG_ENFORCE_SIMPLE = " << Settings.enforce_simple << "\n";
          llvm::errs() << "[Setting] TG_ENFORCE_ARGNUM = " << Settings.enforce_argnum << "\n";*/
        }

        void dumpLLVM(StringRef fname) {
          std::error_code code;
          raw_fd_ostream stream(fname, code);
          if (!code) {
            stream << M;
            stream.close();
          }
        }

        void autodetectSettings() {
          Settings.link_with_libc = M.getFunction("libc_start_init") != nullptr;
          Settings.link_with_compilerrt = M.getFunction("__do_init") != nullptr;
        }

        bool run() {
          if (!Settings.novt_enabled)
            llvm::errs() << "[Setting] NOVT_ENABLED = false\n";
          if (!Settings.enabled) {
            llvm::errs() << "[Setting] TG_ENABLED = false\n";
            return false;
          }
          if (Settings.dump_llvm)
            dumpLLVM("/tmp/before_noic.ll");
          autodetectSettings();
          collectFunctions();
          reportSettings();
          addEnforcement();
          return true;
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
    };

    PreservedAnalyses NoICAbiBuilderPass::run(Module &M, ModuleAnalysisManager &MAM) {
      return NoICPassInternal(M).run() ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    bool NoICAbiBuilderLegacyPass::runOnModule(Module &M) { return NoICPassInternal(M).run(); }

    char NoICAbiBuilderLegacyPass::ID = 0;

    NoICAbiBuilderLegacyPass::NoICAbiBuilderLegacyPass() : ModulePass(ID) {}

    void NoICAbiBuilderLegacyPass::getAnalysisUsage(AnalysisUsage &AU) const {}

    static RegisterPass<NoICAbiBuilderLegacyPass> Registration("NoICAbiBuilder", "NoICAbiBuilderLegacyPass", false, false);

} // namespace llvm