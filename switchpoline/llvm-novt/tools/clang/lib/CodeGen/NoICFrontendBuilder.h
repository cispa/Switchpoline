#ifndef LLVM_NOICFRONTENDBUILDER_H
#define LLVM_NOICFRONTENDBUILDER_H

#include "clang/AST/Expr.h"
#include "clang/AST/GlobalDecl.h"
#include "llvm/IR/Value.h"
#include "clang/AST/ExprCXX.h"
#include <map>

namespace llvm {
    class CallBase;
    class Module;
} // namespace llvm

namespace clang {

    class ASTContext;

    namespace CodeGen {
        class CGCallee;
        class CodeGenModule;
    } // namespace CodeGen

    class FunctionUseRef;

/**
 * The class constructing the Type Graph
 */
    class NoICFrontendBuilder {
        std::map<const llvm::CallBase *, int> uniqueCallNumber;
        std::map<const std::string, int> nextCallNumber;
        std::vector<llvm::CallBase *> calls;
        std::set<const Expr *> ignoreTheseExpressions;

        std::set<FunctionUseRef> functions;
        llvm::DenseSet<const Decl *> usedFunctions;

        bool ignoreFunctionRefUses = false;

        ASTContext &Context;
        clang::CodeGen::CodeGenModule &CGM;

        long nextUniqueNumber = 0;

    public:
        NoICFrontendBuilder(ASTContext &C, clang::CodeGen::CodeGenModule &CGM);
        ~NoICFrontendBuilder();

        void addGlobalDeclaration(GlobalDecl &GD);

        /**
         * Add edge for an (explicit or implicit) type cast expression
         * @param GD
         * @param CE the expression performing the cast
         * @param DestTy the type to cast to
         */
        void addTypeCast(GlobalDecl &GD, const CastExpr *CE, QualType DestTy);

        /**
         * Add edge for an (explicit or implicit type cast
         * @param GD
         * @param E the expression to be casted
         * @param DestTy the type to cast to
         */
        void addTypeCast2(GlobalDecl &GD, const Expr *E, QualType DestTy);

        /**
         * Any kind of cast that is *not* covered by a Cast node in the AST.
         * For example: const cast in initializer lists.
         * @param GD
         * @param E
         * @param DestTy
         */
        void addImplicitTypeCast(GlobalDecl &GD, const Expr *E, QualType DestTy);

        void addAddressOfFunction(GlobalDecl &GD, const UnaryOperator *E);

        void beforeCallArgsEmission(const CallExpr *E);
        void addCall(GlobalDecl &GD, const CallExpr *E,
                     clang::CodeGen::CGCallee &callee, llvm::CallBase *callinst,
                     const Decl *targetDecl);
        void addCXXMemberCall(GlobalDecl &GD, const CallExpr *CE,
                              const CXXMethodDecl *MD, bool isVirtual,
                              const CXXMethodDecl *DevirtMD, const Expr *Base);
        void addCXXConstructorCall(GlobalDecl &GD, const CallExpr *CE,
                                   const CXXConstructorDecl *MD, const Expr *Base);
        void addCXXConstructorCall(GlobalDecl &GD, const CXXConstructExpr *CE,
                                   const CXXConstructorDecl *MD);

        void addBuiltinExpr(GlobalDecl &GD, unsigned BuiltinID, const CallExpr *E);

        void addGlobalVarUse(GlobalDecl &GD, const VarDecl *D, const Expr *E,
                             llvm::Value *V, QualType Ty);

        void addLocalVarDef(GlobalDecl &GD, const VarDecl *D);

        void setIgnoreFunctionRefUse(bool ignore) { ignoreFunctionRefUses = ignore; }
        void addFunctionRefUse(GlobalDecl &GD, const FunctionDecl *D, const Expr *E,
                               llvm::Value *V, QualType Ty);

        void Release(llvm::Module &M, const std::string &OutputFile);

        void addGlobalVarInitializer(const VarDecl &D, const Expr *init);

        void beforeGlobalReplacements(
                llvm::SmallVector<std::pair<llvm::GlobalValue *, llvm::Constant *>, 8>
                &Replacements);

        // save current vardecl
        GlobalDecl CurrentContext;

        // some methods don't have CodeGenModule - use this as a replacement
        static thread_local NoICFrontendBuilder *CurrentInstance;
    };

    class TypeGraphBuilderCurrentContextScope {
        NoICFrontendBuilder &TGB;
        GlobalDecl LastContext;

    public:
        inline TypeGraphBuilderCurrentContextScope(NoICFrontendBuilder &TGB,
                                                   const GlobalDecl &GD)
                : TGB(TGB), LastContext(TGB.CurrentContext) {
          TGB.CurrentContext = GD;
          NoICFrontendBuilder::CurrentInstance = &TGB;
        }
        inline ~TypeGraphBuilderCurrentContextScope() {
          TGB.CurrentContext = LastContext;
          NoICFrontendBuilder::CurrentInstance = nullptr;
        }
    };

} // namespace clang

#endif //LLVM_NOICFRONTENDBUILDER_H
