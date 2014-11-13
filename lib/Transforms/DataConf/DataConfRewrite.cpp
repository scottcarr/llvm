#include <map>
#include <set>
#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
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
#include "llvm/Analysis/MemoryBuiltins.h"
//#include "llvm/DebugInfo.h"
#include "llvm/Transforms/DataConf/DataConfAnalysis.h"
#include "llvm/Transforms/DataConf/DataConfRewrite.h"
#include "llvm/Transforms/DataConf/DataConf.h"

using namespace std;
using namespace llvm;
using namespace DataConfAnalysis;
using namespace DataConfRewrite;
using namespace DataConf;

  ModuleRewrite::ModuleRewrite(ModuleAnalysis &ModAnalysis, Module &m, DataLayout &DL) 
: M(m) 
{
  for (Module::iterator It = M.begin(), Ie = M.end(); It != Ie; ++It) {
    Function &F = *It;
    if (!F.isDeclaration() && !F.getName().startswith("llvm.") &&
        !F.getName().startswith("__llvm__")) {
      FunctionAnalysis &FA = ModAnalysis.getFunctionAnalysis(F);
      RewriteFunction(F, FA, DL, M);
    }
  }
}
void ModuleRewrite::RewriteFunction(Function &Fn, FunctionAnalysis &FA, DataLayout &DL, Module &M) {
  //errs() << "Rewriting: " << Fn.getName() << "\n";
  bool isCPSOnly;
  if (Fn.hasFnAttribute("has-cpi")) {
    isCPSOnly = false;
  } else {
    isCPSOnly = true;
  }
  map<Value*, Value*> replacementMap;
  vector<Instruction*> toDelete;
  CPIInterfaceFunctions IF;
  CreateCPIInterfaceFunctions(&DL, M, IF);
  currFn = &Fn;
  for (inst_iterator It = inst_begin(Fn), Ie = inst_end(Fn); It != Ie; ++It) {
    Instruction *I = &*It;
    if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
      IRBuilder<> IRB(SI);
      Value *v = SI->getValueOperand();
      Value *l = SI->getPointerOperand();

      Protection pVal = FA.getProtection(v);
      Protection pLoc = FA.getProtection(l);

      if (pVal == NEVER && pLoc == NEVER) {
        continue;
      } else if (pVal == ALWAYS && pLoc == ALWAYS) {
        continue;
      } else if (pVal == CONST) {
        // allow constants to be stored anywhere
        continue;
      } else {

        // this is a fail state,
        // the next checks are just for error reporting

        if (pVal == NEVER && pLoc == ALWAYS) {
          I->dump();
          llvm_unreachable("cannot store unprotected value in protected location");
        }

        if (pVal == ALWAYS && pLoc == NEVER) {
          I->dump();
          llvm_unreachable("cannot store unprotected value in protected location");
        }

        llvm_unreachable("should never get here");
      }

      Value *Val = convertToType(IRB.getInt8PtrTy(), v, IRB);
      Value *Loc = convertToType(IRB.getInt8PtrTy()->getPointerTo(), l, IRB);
      uint64_t Size = DL.getTypeStoreSize(v->getType());
      switch (Size) {
        case 8:
          if (isCPSOnly) {
            IRB.CreateCall2(IF.CPISet8Fn, Loc, Val);
          } else {
            // todo assert bounds
            llvm_unreachable("cpi not implemented");
          }
          break;
        case 4:
          IRB.CreateCall2(IF.CPISet4Fn, Loc, Val);
          break;
        case 2:
          IRB.CreateCall2(IF.CPISet2Fn, Loc, Val);
          break;
        case 1:
          IRB.CreateCall2(IF.CPISet1Fn, Loc, Val);
          break;
        default:
          llvm_unreachable("Unhandled size");
      }
    } else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
      IRBuilder<> IRB(LI);
      Value *l = LI->getPointerOperand();

      if (FA.getProtection(l) == NEVER && FA.getProtection(LI) == NEVER) {
        continue;
      }

      if (FA.getProtection(l) == ALWAYS && FA.getProtection(LI) == NEVER) {
        I->dump();
        llvm_unreachable("cannot load protected location into unprotected value");
      }

      if (FA.getProtection(l) == NEVER && FA.getProtection(LI) == ALWAYS) {
        errs() << "In function: "; Fn.dump();
        errs() << "Instruction :";
        I->dump();
        llvm_unreachable("cannot load unprotected location into protected value");
      }

      Value *Loc = convertToType(IRB.getInt8PtrTy()->getPointerTo(), l, IRB);
      Value *MD = IRB.CreateCall(IF.CPIGetMetadataFn, Loc);
      uint64_t Size = DL.getTypeStoreSize(LI->getType());
      Value *SVal;
      switch (Size) {
        case 8:
          SVal = IRB.CreateCall(IF.CPIGetValFn, MD);
          break;
        case 4:
          SVal = IRB.CreateCall2(IF.CPIGetVal4Fn, MD, Loc);
          break;
        case 2:
          SVal = IRB.CreateCall2(IF.CPIGetVal2Fn, MD, Loc);
          break;
        case 1:
          SVal = IRB.CreateCall2(IF.CPIGetVal1Fn, MD, Loc);
          break;
        default:
          llvm_unreachable("loaded size has to be 8, 4, 2, or 1");
      }
      SVal = convertToType(LI->getType(), SVal, IRB);
      replacementMap.insert(pair<Value*, Value*>(LI, SVal));
    } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
      //errs() << "checking call: "; CI->dump();
      Function *F = CI->getCalledFunction();
      if (!F) {
        if (CI->isInlineAsm()) {
          // can't do anything with Assembly ..
          continue;
        }
        Value *v = CI->getCalledValue();
        if (v && v->getType()->getPointerElementType()->isFunctionTy()) {
          if (FunctionType* ft = dyn_cast<FunctionType>(v->getType()->getPointerElementType())) {
            //errs() << "checking call by type: "; CI->dump();
            CheckCallByType(ft, CI, FA);
            continue;
          }
        }
        CI->dump();
        llvm_unreachable("no called function?");
      } else {
        StringRef N = F->getName();

        // malloc's arg can get unprotected, i guess?
        // TODO this should actually be fixed
        if (N.startswith("llvm") ||
            N.startswith("malloc") || 
            N.str().find("safe_malloc") != string::npos ||
            N.str() == "_Znam" ||  // new keyword
            N.str() == "_Znwm"  || // new keyword
            N.startswith("__llvm__cpi")) // assume CPI is correct
        {
          //errs() << "ignoring: "; CI->dump();
          continue;
        }

        // get for our replacement functions
        if (F->getName().str().find("safe_getchar") != string::npos) {
          IRBuilder<> IRB(CI);
          Value *l = CI->getArgOperand(0);
          Value *Loc = convertToType(IRB.getInt8PtrTy()->getPointerTo(), l, IRB);
          IRB.CreateCall(IF.CPIGetCharFn, Loc);
          toDelete.push_back(CI);
          if (FA.getProtection(l) != ALWAYS) {
            llvm_unreachable("can't call safe_getchar with unprotected arg");
          }
        } else if (F->getName().str().find("safe_putchar") != string::npos) {
          IRBuilder<> IRB(CI);
          Value *l = CI->getArgOperand(0);
          Value *Loc = convertToType(IRB.getInt8PtrTy()->getPointerTo(), l, IRB);
          IRB.CreateCall(IF.CPIPutCharFn, Loc);
          toDelete.push_back(CI);
          if (FA.getProtection(l) != ALWAYS) {
            llvm_unreachable("can't call safe_putchar with unprotected arg");
          }
        } else if (F->getName().str().find("safe_read_double") != string::npos) {
          IRBuilder<> IRB(CI);
          Value *l = CI->getArgOperand(0);
          Value *Loc = convertToType(IRB.getInt8PtrTy()->getPointerTo(), l, IRB);
          IRB.CreateCall(IF.CPIReadDoubleFn, Loc);
          toDelete.push_back(CI);
          if (FA.getProtection(l) != ALWAYS) {
            llvm_unreachable("can't call safe_read_double with unprotected arg");
          }
        } else if (F->getName().str().find("safe_write_double") != string::npos) {
          IRBuilder<> IRB(CI);
          Value *l = CI->getArgOperand(0);
          Value *Loc = convertToType(IRB.getInt8PtrTy()->getPointerTo(), l, IRB);
          IRB.CreateCall(IF.CPIWriteDoubleFn, Loc);
          toDelete.push_back(CI);
          if (FA.getProtection(l) != ALWAYS) {
            llvm_unreachable("can't call safe_write_double with unprotected arg");
          }
        } else if (F->getName().str().find("strcpy") != string::npos) {
          // promote strcpy if its args are protected
          // or the dst is protected and the src is a constant
          IRBuilder<> IRB(CI);
          Value *dst = CI->getArgOperand(0);
          Value *src = CI->getArgOperand(1);
          Protection pDst = FA.getProtection(dst);
          Protection pSrc = FA.getProtection(src);
          if (pDst != pSrc) {
            llvm_unreachable("strcpy's args must have same protection");
          }
          if (pDst == ALWAYS && pSrc == ALWAYS) {
            IRB.CreateCall2(IF.CPIStrCpyFn, dst, src);
            toDelete.push_back(CI);
          }
        } else if (F->getName().str().find("memset") != string::npos &&
            FA.getProtection(CI->getArgOperand(0)) == ALWAYS) 
        {
          // promote memset first arg is protected
          IRBuilder<> IRB(CI);
          Value *dst = CI->getArgOperand(0);
          Value *c = CI->getArgOperand(1);
          Value *n = CI->getArgOperand(2);
          IRB.CreateCall3(IF.CPIMemsetFn, dst, c, n);
          toDelete.push_back(CI);
          if (FA.getProtection(CI) == ALWAYS) {
            llvm_unreachable("error: can't use return value of memset as protected");
          }
        } else {
          //errs() << "checking protection for: "; CI->dump();
          //Protection pCall = FA.getProtection(CI);
          //Function::arg_iterator it;
          //size_t i; 
          //for (it = F->arg_begin(), i = 0;
          //    it != F->arg_end();
          //    ++it, ++i) 
          //{
          //  Type *argType = it->getType();
          //  if (FA.isSensitive(argType) && FA.isUnderlyingTypeStruct(argType)) {
          //    if (FA.getProtection(CI->getArgOperand(i)) != ALWAYS) {
          //      CI->dump();
          //      llvm_unreachable("mismatch of arg and formal parameter protection");
          //    }
          //  } else {
          //    if (FA.getProtection(CI->getArgOperand(i)) == ALWAYS) {
          //      CI->dump();
          //      llvm_unreachable("mismatch of arg and formal parameter protection");
          //    }
          //  }
          //}

          //Type *retType = F->getReturnType();
          //if (FA.isSensitive(retType) && FA.isUnderlyingTypeStruct(retType)) {
          //  if (pCall != ALWAYS) {
          //      CI->dump();
          //    llvm_unreachable("mismatch of used value and returned type protection");
          //  } else {
          //    if (pCall == ALWAYS) {
          //      CI->dump();
          //      llvm_unreachable("mismatch of used value and returned type protection");
          //    }
          //  }
          //}
          if (FunctionType *FType = dyn_cast<FunctionType>(CI->getCalledFunction()->getType())) {
            CheckCallByType(FType, CI, FA);
          } else if (FunctionType *FType = dyn_cast<FunctionType>(
                CI->getCalledValue()->getType()->getPointerElementType()))
          {
            CheckCallByType(FType, CI, FA);
          } else {
            CI->dump();
            llvm_unreachable("function not checked.");
          }
        }
      }


      if (!isCPSOnly) {
        // todo assert bounds
        llvm_unreachable("cpi not implemented");
      }
    }
  }
  //errs() << "replacing :\n";
  for (map<Value*, Value*>::iterator it = replacementMap.begin(); 
      it != replacementMap.end();
      ++it)
  {
    //errs() << "  "; it->first->dump();
    //errs() << "      ->      \n";
    //errs() << "  "; it->second->dump();
    it->first->replaceAllUsesWith(it->second);
  }
  for (vector<Instruction*>::iterator it = toDelete.begin();
      it != toDelete.end();
      ++it)
  {
    //(*it)->removeFromParent(); // removeFromParent apparently ruins the module
    (*it)->eraseFromParent();
  }
}

Value* ModuleRewrite::convertToType(Type *targetType, Value* originalVal, IRBuilder<> &IRB) {
  assert(targetType && "targetType shouldn't be null");
  assert(originalVal && "originalVal shouldn't be null");
  Type* originalType = originalVal->getType();
  if (targetType == originalType) {
    return originalVal; 
  }
  if (originalType->isPointerTy() && targetType->isPointerTy()) {
    return IRB.CreateBitCast(originalVal, targetType);
  } else if (originalType->isIntegerTy() && targetType->isPointerTy()) {
    return IRB.CreateIntToPtr(originalVal, targetType);
  } else if (originalType->isPointerTy() && targetType->isIntegerTy()) {
    return IRB.CreatePtrToInt(originalVal, targetType);
  } else if (originalType->isPointerTy() && targetType->isDoubleTy()) {
    Value *i = IRB.CreatePtrToInt(originalVal, IRB.getInt32Ty());
    return IRB.CreateSIToFP(i, targetType);
  } else if (originalType->isDoubleTy() && targetType->isPointerTy()) {
    Value *i = IRB.CreateFPToSI(originalVal, IRB.getInt32Ty());
    Value *ptr = IRB.CreateIntToPtr(i, targetType);
    return IRB.CreateBitCast(ptr, targetType);
  } else {
    assert(0 && "could not convert type");
    return NULL;
  }
}
void ModuleRewrite::CheckCallByType(FunctionType* FType, CallInst *CI, FunctionAnalysis &FA) {
  size_t i;
  FunctionType::param_iterator it;
  for (it = FType->param_begin(), i = 0;
      it != FType->param_end();
      ++it, ++i) 
  {
    // check params
    Value *a = CI->getArgOperand(i);
    Protection pA = FA.getProtection(a);
    bool isParamProtected = FA.isSensitive(*it) && FA.isUnderlyingTypeStruct(*it);
    if ((isParamProtected && pA == NEVER) ||
        (!isParamProtected && pA == ALWAYS)) 
    {
      errs() << "in module: " << M.getModuleIdentifier() << "\n";
      errs() << "function: " << currFn->getName() << "\n";
      errs() << "formal: "; FType->dump(); errs() << "\n";
      errs() << "arg #: " << i << " arg: "; a->dump();
      errs() << "arg protection: " << getString(pA) << "\n";
      errs() << "formal arg is sensitive: " << isParamProtected << "\n";

      llvm_unreachable("arg and formal parameter protection mismatch");
    }

    // check return
    Protection pRet = FA.getProtection(CI);
    bool isReturnProtected = FA.isSensitive(FType->getReturnType()) 
      && FA.isUnderlyingTypeStruct(FType->getReturnType());
    if ((isReturnProtected && pRet == NEVER) ||
        (!isReturnProtected && pRet == ALWAYS)) 
    {
      llvm_unreachable("mismatch of return type and returned value protection");
    }
  }
}

static Function *DataConfRewrite::CheckInterfaceFunction(Constant *FuncOrBitcast) {
  if (isa<Function>(FuncOrBitcast)) return cast<Function>(FuncOrBitcast);
  FuncOrBitcast->dump();
  report_fatal_error("trying to redefine an CPI "
      "interface function");
}

static void DataConfRewrite::CreateCPIInterfaceFunctions(DataLayout *DL, Module &M,
    CPIInterfaceFunctions &IF) {
  LLVMContext &C = M.getContext();
  Type *VoidTy = Type::getVoidTy(C);
  Type *Int8PtrTy = Type::getInt8PtrTy(C);
  Type *Int8PtrPtrTy = Int8PtrTy->getPointerTo();

  Type *Int32Ty = Type::getInt32Ty(C);
  Type *IntPtrTy = DL->getIntPtrType(C);
  Type *SizeTy = IntPtrTy;

  Type *BoundsTy = VectorType::get(IntPtrTy, 2);
  //Type *PtrValBoundsTy = StructType::get(IntPtrTy, IntPtrTy, BoundsTy, NULL);

  IF.CPIInitFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_init", VoidTy, NULL));

  IF.CPISetFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_set", VoidTy, Int8PtrPtrTy, Int8PtrTy, NULL));

  IF.CPIAssertFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_assert", BoundsTy, Int8PtrPtrTy,
        Int8PtrTy, Int8PtrTy, NULL));

  IF.CPISetBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_set_bounds", VoidTy, Int8PtrPtrTy, Int8PtrTy,
        BoundsTy, NULL));

  IF.CPIAssertBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_assert_bounds", VoidTy, Int8PtrTy, SizeTy,
        BoundsTy, Int8PtrTy, NULL));

  IF.CPIGetMetadataFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_metadata", Int8PtrTy, Int8PtrPtrTy, NULL));

  IF.CPIGetMetadataNocheckFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_metadata_nocheck", Int8PtrTy, Int8PtrPtrTy, NULL));

  IF.CPIGetValFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_val", Int8PtrTy, Int8PtrTy, NULL));

  IF.CPIGetBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_bounds", BoundsTy, Int8PtrTy, NULL));

  IF.CPISetArgBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_set_arg_bounds", VoidTy, Int32Ty, BoundsTy, NULL));

  IF.CPIGetArgBoundsFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_arg_bounds", BoundsTy, Int32Ty, NULL));

  IF.CPIDeleteRangeFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_delete_range", VoidTy, Int8PtrTy, SizeTy, NULL));

  IF.CPICopyRangeFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_copy_range", VoidTy, Int8PtrTy, Int8PtrTy, SizeTy, NULL));

  IF.CPIMoveRangeFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_move_range", VoidTy, Int8PtrTy, Int8PtrTy, SizeTy, NULL));

  IF.CPIMallocSizeFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_malloc_size", SizeTy, Int8PtrTy, NULL));

  IF.CPIAllocFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_alloc", VoidTy, Int8PtrTy, NULL));

  IF.CPIReallocFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_realloc", VoidTy, Int8PtrTy, SizeTy,
        Int8PtrTy, SizeTy, NULL));

  IF.CPIFreeFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_free", VoidTy, Int8PtrTy, NULL));

#ifdef CPI_PROFILE_STATS
  IF.CPIRegisterProfileTable = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_register_profile_table", VoidTy, Int8PtrTy, SizeTy, NULL));
#endif

  IF.CPIDumpFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_dump", VoidTy, Int8PtrPtrTy, NULL));

  // DATA_CONF
  IF.CPIMemsetFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_memset", VoidTy, Int8PtrTy, Int32Ty, SizeTy, BoundsTy, NULL));

  IF.CPIGetVal8Fn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_val_8", Int8PtrTy, Int8PtrTy, Int8PtrPtrTy, NULL));

  IF.CPIGetVal4Fn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_val_4", Int8PtrTy, Int8PtrTy, Int8PtrPtrTy, NULL));

  IF.CPIGetVal2Fn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_val_2", Int8PtrTy, Int8PtrTy, Int8PtrPtrTy, NULL));

  IF.CPIGetVal1Fn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_get_val_1", Int8PtrTy, Int8PtrTy, Int8PtrPtrTy, NULL));

  IF.CPISet8Fn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_set_8", VoidTy, Int8PtrPtrTy, Int8PtrTy, NULL));

  IF.CPISet4Fn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_set_4", VoidTy, Int8PtrPtrTy, Int8PtrTy, NULL));

  IF.CPISet2Fn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_set_2", VoidTy, Int8PtrPtrTy, Int8PtrTy, NULL));

  IF.CPISet1Fn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_set_1", VoidTy, Int8PtrPtrTy, Int8PtrTy, NULL));

  IF.CPIStrCpyFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_strcpy", Int8PtrTy, Int8PtrTy, Int8PtrTy, NULL));

  //IF.CPISafe2SafeStrCpyFn = CheckInterfaceFunction(M.getOrInsertFunction(
  //      "__llvm__cpi_safe_to_safe_strcpy", SizeTy, Int8PtrTy, Int8PtrTy, NULL));

  //IF.CPIUnsafe2SafeStrCpyFn = CheckInterfaceFunction(M.getOrInsertFunction(
  //      "__llvm__cpi_unsafe_to_safe_strcpy", SizeTy, Int8PtrTy, Int8PtrTy, NULL));

  //IF.CPISafe2UnsafeStrCpyFn = CheckInterfaceFunction(M.getOrInsertFunction(
  //      "__llvm__cpi_safe_to_unsafe_strcpy", SizeTy, Int8PtrTy, Int8PtrTy, NULL));

  IF.CPIStrLenFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_strlen", SizeTy, Int8PtrTy, NULL));

  IF.CPIGetCharFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_getchar", VoidTy, Int8PtrPtrTy, NULL));

  IF.CPIPutCharFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_putchar", VoidTy, Int8PtrPtrTy, NULL));

  IF.CPIReadDoubleFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_read_double", VoidTy, Int8PtrPtrTy, NULL));

  IF.CPIWriteDoubleFn = CheckInterfaceFunction(M.getOrInsertFunction(
        "__llvm__cpi_write_double", VoidTy, Int8PtrPtrTy, NULL));
  // end DATA_CONF

}
