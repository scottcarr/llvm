#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/PassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/DataConf/DataConf.h"
#include "llvm/Transforms/DataConf/DataConfAnalysis.h"
#include "llvm/Transforms/DataConf/DataConfRewrite.h"

#include <set>

using namespace llvm;
using namespace std;

using namespace DataConfAnalysis;
using namespace DataConfRewrite;
using namespace DataConf;


namespace {
  struct DataConfPass : public ModulePass {
    static char ID;
    set<Type*> sensitiveTypes;
    DataConfPass() : ModulePass(ID) {}
    virtual bool runOnModule(Module &M);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DataLayout>();
      AU.addRequired<TargetLibraryInfo>();
      AU.addRequired<AliasAnalysis>();
    }
  };
}

bool DataConfPass::runOnModule(Module &M) {
  //const unsigned NumCPIGVs = sizeof(CPIInterfaceFunctions)/sizeof(Function*);
  //union {
  //  CPIInterfaceFunctions IF;
  //  GlobalValue *GV[NumCPIGVs];
  //};

  //CreateCPIInterfaceFunctions(&getAnalysis<DataLayout>(), M, IF);

  //Type *Int8PtrTy = Type::getInt8PtrTy(M.getContext());
  //for (unsigned i = 0; i < NumCPIGVs; ++i) {
  //  if (GV[i]) appendToGlobalArray(M, "llvm.compiler.used",
  //                      ConstantExpr::getBitCast(GV[i], Int8PtrTy));
  //}

  //M.getGlobalVariable("llvm.compiler.used")->setSection("llvm.metadata");

  bool changed = false;
  //DataLayout &dl = getAnalysis<DataLayout>();
  //TargetLibraryInfo &tli = getAnalysis<TargetLibraryInfo>();
  //AliasAnalysis &aa = getAnalysis<AliasAnalysis>();
  //DataConfAnalysis::ModuleAnalysis ma(M);
  //DataConfRewrite::ModuleRewrite(ma, M, dl);

  //std::string errorStr;
  dumpModule(M);
  return changed;
}

char DataConfPass::ID = 0;
static RegisterPass<DataConfPass> X("DataConf", "Data Confidentiality Pass", false, false);

