#ifndef LLVM_NOICDISPATCHERBUILDER_H
#define LLVM_NOICDISPATCHERBUILDER_H

#ifndef WITHOUT_LLVM

#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/CustomSettings.h>

#endif

#include <set>
#include <string>
#include <vector>

using namespace typegraph;

constexpr long NOID = -1;

template<class FunctionRef, class UseRef>
struct TGDFunction {
    long ID;
    bool Leaking;
    FunctionRef Ref;
    // std::vector<UseRef> Uses;

    bool Removed = false;
    bool InCluster = false;

    TGDFunction(long Id, FunctionRef Ref) : ID(Id), Ref(Ref) {}
};

enum TGDResolvePointType {
    CALL, RESOLVE_FUNCTION, EXTERNAL_FUNCTION
};

/**
 * A resolve point is one of:
 * - an indirect call (converted to switch case)
 * - a call to stdlib with function pointer argument (needs raw pointer)
 * @tparam CallRef
 */
template<class FunctionRef, class UseRef, class CallRef>
struct TGDResolvePoint {
    TGDResolvePointType Type;
    const std::string *CallName;
    bool IsExternal = false;
    std::vector<TGDFunction<FunctionRef, UseRef> *> Targets;
    CallRef CallInst;
    int ResolveArgnum;

    long VertexID = -1;
    std::string Hash = "";

    TGDResolvePoint(TGDResolvePointType Type, const std::string *CallName, CallRef CallInst)
            : Type(Type), CallName(CallName), CallInst(CallInst) {}
};

template<class FunctionRef, class UseRef, class CallRef>
class TGDispatcherBuilder {
public:
    typedef TGDFunction<FunctionRef, UseRef> DFunction;
    typedef TGDResolvePoint<FunctionRef, UseRef, CallRef> DResolvePoint;

protected:
    std::map<FunctionRef, std::unique_ptr<DFunction>> Functions;
    std::vector<DResolvePoint> ResolvePoints;
    long NextId = 1;

public:
    /**
     * Add a call-typed resolve point.
     * @return A reference to the new resolvepoint. Rendered invalid when the next add occurs.
     */
    DResolvePoint &addCall(const std::string *CallName, CallRef Ref) {
      ResolvePoints.emplace_back(CALL, CallName, Ref);
      return ResolvePoints.back();
    }

    DResolvePoint &addResolvePoint(const std::string *RPName, CallRef Ref, int ArgIndex) {
      ResolvePoints.emplace_back(EXTERNAL_FUNCTION, RPName, Ref);
      ResolvePoints.back().ResolveArgnum = ArgIndex;
      return ResolvePoints.back();
    }

    DResolvePoint &addResolveFunction(const std::string *RPName, CallRef Ref) {
      ResolvePoints.emplace_back(RESOLVE_FUNCTION, RPName, Ref);
      return ResolvePoints.back();
    }

    DFunction *getFunction(FunctionRef Ref, long ID = NOID) {
      auto It = Functions.find(Ref);
      if (It == Functions.end()) {
        DFunction *F = (Functions[Ref] = std::make_unique<DFunction>(ID, Ref)).get();
        F->Leaking = false;
        return F;
      }
      return It->second.get();
    }

    DFunction *getFunctionOpt(FunctionRef Ref) {
      auto It = Functions.find(Ref);
      if (It == Functions.end()) {
        return nullptr;
      }
      return It->second.get();
    }

    /*
    void purgeUnusedFunctions() {
      for (auto &F : Functions) {
        if (F.Uses.empty())
          F.Removed = true;
      }
      for (auto &RP : ResolvePoints) {
        // TODO remove F from RP.Targets if F.Removed
      }
    }*/

};

#ifndef WITHOUT_LLVM

struct FunctionToIDReplacement {
    llvm::User *User;
    llvm::Value *Old;
};

class LLVMDispatcherBuilder : public TGDispatcherBuilder<llvm::Function *, llvm::User *, llvm::CallBase *> {
    llvm::LLVMContext &Context;
    llvm::Module &M;
    llvm::IntegerType *IntPtr;
    std::map<llvm::CallBase *, const std::string *> DynamicSymbolCalls;

public:
    size_t ModuleID = 1;
    llvm::DenseMap<const std::string *, llvm::GlobalVariable *> ExternalCallDispatchers;
    llvm::DenseMap<const std::string *, llvm::GlobalVariable *> ExternalDynamicSymbolInfos;
    llvm::DenseMap<const std::string *, std::set<std::string>> DynamicSymbolTargets;
    std::map<const std::string, long> CountPerTargetHash;

    inline LLVMDispatcherBuilder(llvm::LLVMContext &Context, llvm::Module &M)
            : Context(Context), M(M) {
      NextId = Settings.enforce_min_id;
      IntPtr = M.getDataLayout().getIntPtrType(M.getContext());
    }

    inline void addDynamicSymbolCall(llvm::CallBase *DynSymbolCall, const std::string *DlSymName) {
      DynamicSymbolCalls[DynSymbolCall] = DlSymName;
    }

    void initialize();

    void assignOptimalIDs();

    void assignMissingIDs();

    void debugIDAssignment();

    void replaceFunctionsWithIDs();

    void generateCodeForResolvePoint(DResolvePoint &RP);

    inline void generateCodeForAll() {
      for (auto &RP: ResolvePoints) {
        generateCodeForResolvePoint(RP);
      }
    }

    void generateCodeForDynamicSymbols();

    void printFunctionIDs();

    void writeFunctionIDsToFile();

    inline std::map<llvm::Function *, std::unique_ptr<DFunction>> &getFunctions() {
      return Functions;
    }

    inline void setModuleID(size_t NewModuleID) {
      this->ModuleID = NewModuleID;
      if (ModuleID > typegraph::Settings.enforce_min_id) {
        NextId = ModuleID;
      } else {
        NextId = typegraph::Settings.enforce_min_id;
      }
    }

    void allTrampolinesGenerated();

    void writeStatistics(const std::string &Filename);

    long getNextId();

private:
    bool typesafeReplaceAllUsesWith(llvm::User *OldValue, llvm::Constant *NewValue);

    bool typesafeReplaceUseWith(llvm::User *User, llvm::Value *Old, llvm::Constant *New);

    std::vector<FunctionToIDReplacement> getFunctionUsages(llvm::Value *F);

    bool canReplaceAllFunctionUsages(llvm::Value *V);

    void collectCluster(std::set<DFunction *> &Cluster, std::set<DResolvePoint *> &RPs);

    long assignClusterIDs(std::set<DFunction *> &Cluster, std::set<DResolvePoint *> &RPs, long MinNumber);

    // generate code that translates a function ID back to a pointer
    llvm::Value *generateBackTranslation(DResolvePoint &RP, llvm::Value *OldValue, llvm::Instruction *SplitBefore);

    // generate code that errors
    void generateErrorCase(llvm::IRBuilder<> &Builder, DResolvePoint &RP, llvm::Value *FunctionID);

    void newTrampoline(llvm::GlobalVariable *GV);
};

#endif

#endif // LLVM_NOICDISPATCHERBUILDER_H