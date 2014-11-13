#ifndef __DATACONFREWRITE_INC__
#define __DATACONFREWRITE_INC__

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/DataConf/DataConfAnalysis.h"

using namespace DataConfAnalysis;
using namespace llvm;

namespace DataConfRewrite {
  class ModuleRewrite {
    Module &M;
    Function *currFn;
    public:
      ModuleRewrite(ModuleAnalysis &ModAnalysis, Module &m, DataLayout &DL);
    private:
      void RewriteFunction(Function &Fn, FunctionAnalysis &FA, DataLayout &DL, Module &M);
      static Value* convertToType(Type *targetType, Value* originalVal, IRBuilder<> &IRB);
      void CheckCallByType(FunctionType* FType, CallInst *CI, FunctionAnalysis &FA);
  };
  Function *CheckInterfaceFunction(Constant *FuncOrBitcast);

  struct CPIInterfaceFunctions {
    Function *CPIInitFn;

    Function *CPISetFn;
    Function *CPIAssertFn;

    Function *CPISetBoundsFn;
    Function *CPIAssertBoundsFn;

    Function *CPIGetMetadataFn;
    Function *CPIGetMetadataNocheckFn;
    Function *CPIGetValFn;
    Function *CPIGetBoundsFn;

    Function *CPISetArgBoundsFn;
    Function *CPIGetArgBoundsFn;

    Function *CPIDeleteRangeFn;
    Function *CPICopyRangeFn;
    Function *CPIMoveRangeFn;

    Function *CPIMallocSizeFn;
    Function *CPIAllocFn;
    Function *CPIReallocFn;
    Function *CPIFreeFn;

#ifdef CPI_PROFILE_STATS
    Function *CPIRegisterProfileTable;
#endif

    Function *CPIDumpFn;

    // data conf
    Function *CPIMemsetFn;

    Function *CPIGetVal8Fn;
    Function *CPIGetVal4Fn;
    Function *CPIGetVal2Fn;
    Function *CPIGetVal1Fn;

    Function *CPISet8Fn;
    Function *CPISet4Fn;
    Function *CPISet2Fn;
    Function *CPISet1Fn;

    Function *CPIStrCpyFn;
    Function *CPIStrLenFn;

    Function *CPIGetCharFn;
    Function *CPIPutCharFn;
    Function *CPIReadDoubleFn;
    Function *CPIWriteDoubleFn;
  };
  void CreateCPIInterfaceFunctions(DataLayout *DL, 
      Module &M, 
      CPIInterfaceFunctions &IF);
}
#endif
