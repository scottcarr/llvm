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
#include "llvm/IR/InstIterator.h"
#include "llvm/PassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/ProtInf/ProtInf.h"

#include <set>

using namespace llvm;
using namespace std;


namespace {
  struct ProtInfPass : public ModulePass {
    static char ID;
    set<Type*> sensitiveTypes;
    ProtInfPass() : ModulePass(ID) {}
    virtual bool runOnModule(Module &M);
  };
}

bool ProtInfPass::runOnModule(Module &M) {
  bool changed = false;
  ModuleInf mi(M);
  //mi.dumpConstraints();
  return changed;
}

char ProtInfPass::ID = 0;
static RegisterPass<ProtInfPass> X("ProtInf", "Protection Inference Pass", false, false);

