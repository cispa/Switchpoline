#include "NoICFrontendBuilder.h"
#include "CGCall.h"
#include "CodeGenModule.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Basic/Builtins.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CustomSettings.h"
#include <fstream>
#include <functional>

namespace {
    inline bool startsWith(const std::string &check, const std::string &prefix) {
      return std::equal(prefix.begin(), prefix.end(), check.begin());
    }

    inline bool endsWith(std::string const &fullString, std::string const &ending) {
      if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(),
                                        ending.length(), ending));
      }
      return false;
    }

    llvm::Value *pullFunctionFromValue(llvm::Value *V) {
      if (auto *V2 = llvm::dyn_cast<llvm::BitCastOperator>(V)) {
        V = V2->getOperand(0);
      }
      return V;
    }

} // namespace

using namespace clang::CodeGen;

namespace clang {

    struct FunctionUseRef {
        const NamedDecl *functionDecl;
        llvm::Value *functionRef;

        FunctionUseRef(const NamedDecl *functionDecl, llvm::Value *fr)
                : functionDecl(functionDecl), functionRef(fr) {}

        inline bool operator==(const FunctionUseRef &other) const {
          return functionRef == other.functionRef &&
                 functionDecl == other.functionDecl;
        }
        inline bool operator<(const FunctionUseRef &other) const {
          if (functionRef == other.functionRef)
            return functionDecl < other.functionDecl;
          return functionRef < other.functionRef;
        }
    };

    class Serializer {
    public:
        std::string module_unique_value;
        const std::map<const llvm::CallBase *, int> &call_numbers;
        CodeGenModule &CGM;

        Serializer(llvm::Module &M, CodeGenModule &CGM,
                   const std::map<const llvm::CallBase *, int> &call_numbers)
                : module_unique_value(M.getName()), call_numbers(call_numbers), CGM(CGM) {
        }

        std::string serialize(const llvm::CallBase *call) {
          auto funcname = call->getFunction()->getName().str();
          if (call->getFunction()->hasLocalLinkage()) {
            funcname = "internal " + funcname + "@" + module_unique_value;
          }
          const auto &it = call_numbers.find(call);
          auto number = it != call_numbers.cend() ? std::to_string(it->second)
                                                  : std::to_string((uintptr_t)call);
          return "call#" + number + " in " + funcname;
        }

        /**
         * Serialize resolve points
         * @param call
         * @param argnum
         * @return
         */
        std::string serialize(const llvm::CallBase *call, int argnum) {
          auto funcname = call->getFunction()->getName().str();
          if (call->getFunction()->hasLocalLinkage()) {
            funcname = "internal " + funcname + "@" + module_unique_value;
          }
          const auto &it = call_numbers.find(call);
          auto number = it != call_numbers.cend() ? std::to_string(it->second)
                                                  : std::to_string((uintptr_t)call);
          return "resolvepoint#" + std::to_string(call->getValueID()) + "." +
                 number + "." + std::to_string(argnum) + " in " + funcname;
        }

        std::string serializeDynamicFunction(const llvm::CallBase *call) {
          auto funcname = call->getFunction()->getName().str();
          if (call->getFunction()->hasLocalLinkage()) {
            funcname = "internal " + funcname + "@" + module_unique_value;
          }
          const auto &it = call_numbers.find(call);
          auto number = it != call_numbers.cend() ? std::to_string(it->second)
                                                  : std::to_string((uintptr_t)call);
          return "dynamicsymbol#" + std::to_string(call->getValueID()) + "." +
                 number + " in " + funcname;
        }

        std::string serialize(const NamedDecl *ND) {
          std::string name = "unknown";
          bool isInternal = !ND->isExternallyVisible();
          if (const auto *CD = dyn_cast_or_null<CXXConstructorDecl>(ND)) {
            name = CGM.getMangledName(GlobalDecl(CD, CXXCtorType::Ctor_Base));
          } else if (const auto *DD = dyn_cast_or_null<CXXDestructorDecl>(ND)) {
            name = CGM.getMangledName(GlobalDecl(DD, CXXDtorType::Dtor_Base));
          } else if (const auto *FD = dyn_cast_or_null<FunctionDecl>(ND)) {
            name = CGM.getMangledName(GlobalDecl(FD)).str();
          } else if (const auto *VD = dyn_cast_or_null<VarDecl>(ND)) {
            name = CGM.getMangledName(GlobalDecl(VD)).str();
            if (VD->getLinkageAndVisibility().getLinkage() ==
                Linkage::InternalLinkage ||
                VD->isInAnonymousNamespace())
              isInternal = true;
          } else if (ND->getDeclName().isIdentifier()) {
            name = ND->getName().str();
          } else {
            llvm::errs() << "Unknown named decl: ";
            ND->dump(llvm::errs());
          }
          if (isInternal) {
            name = "internal " + name + "@" + module_unique_value;
          }
          return name;
        }

        std::string serialize(const FunctionUseRef &use) {
          // must match "declaration" serialization above, including mangling,
          // namespaces and "internal"
          auto name = serialize(use.functionDecl);
          if (name == "unknown" && use.functionRef && use.functionRef->hasName())
            return "symbol " + use.functionRef->getName().str();
          return name;
        }
    };

    thread_local NoICFrontendBuilder *NoICFrontendBuilder::CurrentInstance = nullptr;

    void NoICFrontendBuilder::addGlobalDeclaration(GlobalDecl &GD) {}

    void NoICFrontendBuilder::addTypeCast(GlobalDecl &GD, const CastExpr *CE,
                                          QualType DestTy) {}

    void NoICFrontendBuilder::addTypeCast2(GlobalDecl &GD, const Expr *E, QualType DestTy) {}

    void NoICFrontendBuilder::addImplicitTypeCast(GlobalDecl &GD, const Expr *E,
                                                  QualType DestTy) {}

    void NoICFrontendBuilder::addAddressOfFunction(GlobalDecl &GD,
                                                   const UnaryOperator *E) {
      if (!GD.getDecl())
        GD = CurrentContext;
      assert(GD.getDecl());
      // TODO record function use
    }

    void NoICFrontendBuilder::beforeCallArgsEmission(const CallExpr *E) {}

    void NoICFrontendBuilder::addCall(GlobalDecl &GD, const CallExpr *E,
                                      CodeGen::CGCallee &callee,
                                      llvm::CallBase *callinst,
                                      const Decl *targetDecl) {
      if (!GD.getDecl())
        GD = CurrentContext;
      assert(GD.getDecl());
      const auto *functionDecl = dyn_cast_or_null<FunctionDecl>(targetDecl);

      // save type for call
      if (!functionDecl) {

        auto num = nextCallNumber[callinst->getFunction()->getName().str()]++;
        uniqueCallNumber[callinst] = num;
        calls.push_back(callinst);
      }
    }

    void NoICFrontendBuilder::addCXXMemberCall(GlobalDecl &GD, const CallExpr *CE,
                                               const CXXMethodDecl *MD, bool isVirtual,
                                               const CXXMethodDecl *DevirtMD,
                                               const Expr *Base) {
      if (!GD.getDecl())
        GD = CurrentContext;
      assert(GD.getDecl());
      // TODO handle member function pointers
    }

    void NoICFrontendBuilder::addCXXConstructorCall(GlobalDecl &GD, const CallExpr *CE,
                                                    const CXXConstructorDecl *MD,
                                                    const Expr *Base) {}

    void NoICFrontendBuilder::addCXXConstructorCall(GlobalDecl &GD,
                                                    const CXXConstructExpr *CE,
                                                    const CXXConstructorDecl *MD) {}

    void NoICFrontendBuilder::addBuiltinExpr(GlobalDecl &GD, unsigned BuiltinID, const CallExpr *E) {}

    void NoICFrontendBuilder::addGlobalVarUse(GlobalDecl &GD, const VarDecl *D,
                                              const Expr *E, llvm::Value *V, QualType Ty) {}

    void NoICFrontendBuilder::addFunctionRefUse(GlobalDecl &GD, const FunctionDecl *D,
                                                const Expr *E, llvm::Value *V, QualType Ty) {
      if (ignoreFunctionRefUses)
        return;
      if (!GD.getDecl())
        GD = CurrentContext;
      assert(GD.getDecl());
      V = pullFunctionFromValue(V);
      // llvm::errs() << "[BUILDER] Use of function " << D->getName() << "\n";
      if (V && !isa<llvm::Function>(V))
        llvm::errs() << "[WARNING] addFunctionRefUse: V is not a function! " << *V << "\n";
      functions.emplace(D, V);
      while (D->getPreviousDecl())
        D = D->getPreviousDecl();
      usedFunctions.insert(D);
    }

    void NoICFrontendBuilder::beforeGlobalReplacements(
            llvm::SmallVector<std::pair<llvm::GlobalValue *, llvm::Constant *>, 8>
            &Replacements) {
      for (auto &I : Replacements) {
        llvm::GlobalValue *GV = I.first;
        llvm::Constant *C = I.second;
        auto it = functions.begin();
        while (it != functions.end()) {
          if (it->functionRef == GV) {
            functions.insert(FunctionUseRef(it->functionDecl, pullFunctionFromValue(C)));
            it = functions.erase(it);
          } else {
            it++;
          }
        }
      }
    }

    NoICFrontendBuilder::NoICFrontendBuilder(ASTContext &C, CodeGenModule &CGM)
            : Context(C), CGM(CGM) {}

    NoICFrontendBuilder::~NoICFrontendBuilder() {}

    void NoICFrontendBuilder::Release(llvm::Module &M, const std::string &OutputFile) {
      // FIX BUG FROM MUSL LIBC
      auto *F = M.getFunction("__dls2");
      if (F) {
        F->setVisibility(llvm::GlobalValue::DefaultVisibility);
      }

      // Give all calls some name
      Serializer serializer(M, CGM, uniqueCallNumber);
      for (auto *Call: calls) {
        auto name = serializer.serialize(Call);
        auto *MD = llvm::MDNode::get(M.getContext(), {llvm::MDString::get(M.getContext(), name)});
        Call->setMetadata(llvm::LLVMContext::MD_call_name, MD);
      }
    }

    namespace {
        class ConstExprParser : public StmtVisitor<ConstExprParser, void, QualType> {
            NoICFrontendBuilder &TGB;
            CodeGenModule &CGM;
            GlobalDecl GD;

        public:
            ConstExprParser(NoICFrontendBuilder &tgb, CodeGenModule &CGM, GlobalDecl GD)
                    : TGB(tgb), CGM(CGM), GD(GD) {}

            void VisitArraySubscriptExpr(ArraySubscriptExpr *E, QualType T) {
              // Auto-generated version seems to be broken.
              Visit(E->getBase(), E->getBase()->getType());
              Visit(E->getIdx(), E->getIdx()->getType());
            }

            void VisitCastExpr(CastExpr *CE, QualType T) {
              TGB.addTypeCast(GD, CE, T);
              Visit(CE->getSubExpr(), CE->getSubExpr()->getType());
            }

            void VisitUnaryAddrOf(UnaryOperator *E, QualType T) {
              if (T->isFunctionPointerType() &&
                  E->getSubExpr()->getType()->isFunctionType()) {
                TGB.addAddressOfFunction(GD, E);
                if (T != E->getType()) {
                  TGB.addTypeCast2(GD, E, T);
                }
              }
              Visit(E->getSubExpr(), E->getSubExpr()->getType());
            }

            void VisitDeclRefExpr(DeclRefExpr *E, QualType T) {
              if (auto *FD = dyn_cast<FunctionDecl>(E->getDecl())) {
                auto *V = CGM.GetAddrOfFunction(FD);
                TGB.addFunctionRefUse(GD, FD, E, V, FD->getType());
              } else if (auto *GV = dyn_cast<VarDecl>(E->getDecl())) {
                if (!GV->isLocalVarDeclOrParm()) {
                  TGB.addGlobalVarUse(GD, GV, E, nullptr, T);
                }
              }
            }

            void VisitInitListExpr(InitListExpr *ILE, QualType T) {
              for (unsigned i = 0; i < ILE->getNumInits(); i++) {
                // Check if we have a "const struct" with non-qualified initializer fields
                // happens C++ only so far
                auto Ty = ILE->getInit(i)->getType();
                Ty.addFastQualifiers(T.getCVRQualifiers());
                /*if (Ty != ILE->getInit(i)->getType()) {
                  TGB.addTypeCast2(GD, ILE->getInit(i), Ty);
                }*/
                Visit(ILE->getInit(i), Ty);
              }
            }

            void VisitParenExpr(ParenExpr *E, QualType T) {
              Visit(E->getSubExpr(), T);
            }
        };
    } // namespace

    void NoICFrontendBuilder::addLocalVarDef(GlobalDecl &GD, const VarDecl *D) {
      if (D->getInit()) {
        ConstExprParser(*this, CGM, GD).Visit(const_cast<Expr *>(D->getInit()), D->getType());
      }
    }

    void NoICFrontendBuilder::addGlobalVarInitializer(const VarDecl &D,
                                                      const Expr *init) {
      if (!init)
        return;
      ConstExprParser(*this, CGM, GlobalDecl(&D)).Visit(const_cast<Expr *>(init), D.getType());
    }

} // namespace clang