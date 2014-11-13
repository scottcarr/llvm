#ifndef __DATACONFANALY__
#define __DATACONFANALY__
#include <set>
#include <map>
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
//#include "llvm/Transforms/DataConf/DataConfRewrite.h"
#include "llvm/Transforms/DataConf/DataConfAnalysis.h"

using namespace std;
using namespace llvm;
using namespace DataConf;
using namespace DataConfAnalysis;

void DataConf::dumpModule(Module &M) {
  std::error_code ec;
  StringRef name = "a";
  std::string mname = name.str() + "_analyze.ll";
  //raw_fd_ostream fout(mname.c_str(), errorStr);
  raw_fd_ostream fout(StringRef(mname), ec, sys::fs::F_None);
  M.print(fout, NULL);
  fout.close();
}

string DataConfAnalysis::getString(Protection p) {
  switch (p) {
    case ALWAYS:
      return "ALWAYS";
    case NEVER:
      return "NEVER";
    case MAYBE:
      return "MAYBE";
    default:
      llvm_unreachable("not getstring for unknown protection");
  }
}
bool FunctionAnalysis::isProtectedStruct(Value* val) {
  Type* type = val->getType();
  if (isSensitive(type) && isUnderlyingTypeStruct(type)) {
    return true;
  }
  return false;
}
void FunctionAnalysis::update(Value* I, Protection status) {
  if (isa<ConstantExpr>(I) && isa<Operator>(I)) {
    if (Operator *op = cast<Operator>(I)) { 
      switch (op->getOpcode()) {
        case Instruction::BitCast:
        case Instruction::GetElementPtr:
          update(op->getOperand(0), status);
          return;
        default:
          errs() << "in module: " << M->getModuleIdentifier() << "\n";
          errs() << "in function: " << F->getName() << "\n";
          errs() << "opcode: " << op->getOpcode() << "\n";
          op->dump();
          llvm_unreachable("unhandled op code in update");
      }
    }
  }
  Container_t::iterator it;
  if (isa<GlobalVariable>(I) || isa<GlobalValue>(I)) {
    it = globals.find(I);
  } else if (isa<ConstantInt>(I)) { 
    return;
  } else if (isa<ConstantFP>(I)) { 
    return;
  } else if (isa<ConstantPointerNull>(I)) { 
    return;
  } else {
    it = locals.find(I);
  }
  if (it == locals.end() || it == globals.end()) {
    errs() << "in module: " << M->getModuleIdentifier() << "\n";
    errs() << "in function: " << F->getName() << "\n";
    I->dump();
    llvm_unreachable("value not found");
  } else if (it->second == NEVER && status == ALWAYS) {
    // for now we only have a one way change NEVER->ALWAYS
    it->second = status;
    isChanged = true;
  } 
}
void FunctionAnalysis::protect3OpInst(Instruction *I) {
  Value *op0 = I->getOperand(0);
  Value *op1 = I->getOperand(1);
  Value *op2 = I->getOperand(2);
  Protection p0 = getProtection(op0);
  Protection p1 = getProtection(op1);
  Protection p2 = getProtection(op2);
  Protection pI = getProtection(I);
  if (p0 == ALWAYS || p1 == ALWAYS || p2 == ALWAYS || pI == ALWAYS) {
    update(I, ALWAYS);
    update(op0, ALWAYS);
    update(op1, ALWAYS);
    update(op2, ALWAYS);
  } 
}
void FunctionAnalysis::protect2OpInst(Instruction *I) {
  Value *op0 = I->getOperand(0);
  Value *op1 = I->getOperand(1);
  Protection p0 = getProtection(op0);
  Protection p1 = getProtection(op1);
  Protection pI = getProtection(I);
  if (p0 == ALWAYS || p1 == ALWAYS || pI == ALWAYS) {
    update(I, ALWAYS);
    update(op0, ALWAYS);
    update(op1, ALWAYS);
  } 
}
void FunctionAnalysis::protect1OpInst(Instruction *I) {
  Value *op0 = I->getOperand(0);
  Protection p0 = getProtection(op0);
  Protection pI = getProtection(I);
  if (p0 == ALWAYS || 
      pI == ALWAYS ||
      isProtectedStruct(op0)) {
    update(I, ALWAYS);
    update(op0, ALWAYS);
  }
}
void FunctionAnalysis::analyze() {
  do {
    resetIsChanged();
    for (inst_iterator It = inst_begin(F), Ie = inst_end(F); It != Ie; ++It) {
      Instruction *I = &*It;
      //if (F->getName().startswith("MD5")) {
      //  errs() << "analzying: "; I->dump();
      //}
      if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Value *ptrOp = SI->getPointerOperand();
        Value *valOp = SI->getValueOperand();
        if (isProtectedStruct(ptrOp)) {
          update(ptrOp, ALWAYS);
          update(valOp, ALWAYS);
          update(SI, ALWAYS);
        } else {
          protect2OpInst(SI);
        }
      } else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        Value *ptrOp = LI->getPointerOperand();
        if (isProtectedStruct(ptrOp)) {
          update(ptrOp, ALWAYS);
          update(LI, ALWAYS);
        } else {
          protect1OpInst(LI);
        }
      //} else if (ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
      } else if (isa<ReturnInst>(I)) {
        // we might want to do something special with the return 
        // values in the future?
      } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
        // manually set the protection for CPI intrinsics
        Function *Fn = CI->getCalledFunction();
        if (!Fn) {
          Value *v = CI->getCalledValue();
          if (v && v->getType()->getPointerElementType()->isFunctionTy()) {
            if (FunctionType* ft = dyn_cast<FunctionType>(v->getType()->getPointerElementType())) {
              FunctionType::param_iterator it;
              size_t i; 
              for (it = ft->param_begin(), i = 0;
                  it != ft->param_end();
                  ++it, ++i) 
              {
                Type *ty = *it;
                if (isSensitive(ty) && isUnderlyingTypeStruct(ty)) {
                  update(CI->getArgOperand(i), ALWAYS);
                }
              }
              Type *retTy = ft->getReturnType();
              if (isSensitive(retTy) && isUnderlyingTypeStruct(retTy)) {
                update(CI, ALWAYS);
              }
            }
            continue;
          } else if (CI->isInlineAsm()) {
            // nothing we can do, I hope they know what they are doing ...
            continue;
          } else {
            v->dump();
            CI->dump();
            llvm_unreachable("no called function?");
          }
        }
        StringRef N = Fn->getName();
        if (N.str().find("safe_getchar") != string::npos) {
          update(CI->getArgOperand(0), ALWAYS);
          update(CI, ALWAYS);
        } else if (N.str().find("safe_putchar") != string::npos) {
          update(CI->getArgOperand(0), ALWAYS);
          update(CI, ALWAYS);
        } else if (N.str().find("safe_read_double") != string::npos) {
          update(CI->getArgOperand(0), ALWAYS);
          update(CI, ALWAYS);
        } else if (N.str().find("safe_write_double") != string::npos) {
          update(CI->getArgOperand(0), ALWAYS);
          update(CI, ALWAYS);
        } else if (Fn->getName().str().find("safe_malloc") != string::npos) {
          update(CI, ALWAYS);
        } else {
          // todo for other functions, if the return 
          // or args are protected structs set the
          // protections accordingly
          Function::arg_iterator it;
          size_t i; 
          for (it = Fn->arg_begin(), i = 0;
              it != Fn->arg_end();
              ++it, ++i) 
          {
            Type *argType = it->getType();
            if (isSensitive(argType) && isUnderlyingTypeStruct(argType)) {
              update(CI->getArgOperand(i), ALWAYS);
            }
          }
          Type *retType = Fn->getReturnType();
          if (isSensitive(retType) && isUnderlyingTypeStruct(retType)) {
            update(CI, ALWAYS);
          }
        }

        //// don't touch internal functions
        //Function *F = CI->getCalledFunction();
        //StringRef N = F->getName();
        //if (N.startswith("llvm") ||
        //    F->isDeclaration()) 
        //{
        //  continue;
        //}

        //bool protect = false;
        //for (size_t i = 0; i < CI->getNumArgOperands(); ++i) {
        //  Value *a = CI->getArgOperand(i);
        //  if (getProtection(a) == ALWAYS) {
        //    protect = true;
        //    break; 
        //  }
        //}
        //if (protect) {
        //  update(CI, ALWAYS);
        //}
        //
        //// our built in functions can generate safe
        //// values
      //} else if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
      } else if (isa<AllocaInst>(I)) {
        // this variable is allocated on the stack which is safe
      } else if (isa<BranchInst>(I)) {
      //} else if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
      } else if (PHINode *Phi = dyn_cast<PHINode>(I)) {
        // if any incoming edge is protected, protect all
        bool protect = false;
        if (getProtection(Phi) == ALWAYS) { protect = true; }
        for (uint32_t i = 0; i < Phi->getNumOperands(); ++i) {
          if (getProtection(Phi->getOperand(i)) == ALWAYS) {
            protect = true;
            break;
          }
        }
        if (protect) {
          update(Phi, ALWAYS);
          for (uint32_t i = 0; i < Phi->getNumOperands(); ++i) {
            update(Phi->getOperand(i), ALWAYS);
          }
        }
      } else {
        switch(I->getOpcode()) {
          case Instruction::Add:
          case Instruction::FAdd:
          case Instruction::Sub:
          case Instruction::FSub:
          case Instruction::Mul:
          case Instruction::FMul:
          case Instruction::FDiv:
          case Instruction::UDiv:
          case Instruction::SDiv:
          case Instruction::FCmp:
          case Instruction::ICmp:
          case Instruction::Xor:
          case Instruction::Or:
          case Instruction::And:
            {
              protect2OpInst(I);
              break;
            }
          case Instruction::SExt:
          case Instruction::ZExt:
          case Instruction::Trunc:
          case Instruction::Ret:
          case Instruction::BitCast:
          case Instruction::GetElementPtr:
          case Instruction::IntToPtr:
          case Instruction::PtrToInt:
          case Instruction::Shl:
          case Instruction::SRem:
          case Instruction::URem:
          case Instruction::AShr:
          case Instruction::LShr:
          case Instruction::ExtractValue:
          case Instruction::ExtractElement:
          case Instruction::InsertElement:
          case Instruction::SIToFP:
          case Instruction::FPExt:
            {
              protect1OpInst(I);
              break;
            }
          case Instruction::Select: 
            {
              protect3OpInst(I);
              break;
            }
          case Instruction::Unreachable:
          case Instruction::Switch:
            break;
          default:
            errs() << "in module: " << M->getModuleIdentifier() << "\n";
            errs() << "in function: " << F->getName() << "\n";
            I->dump();
            llvm_unreachable("unhandled instruction type");
        }
      }
    }
  } while (getIsChanged());
}
bool FunctionAnalysis::isSensitive(Type* type) {
  return sensitiveTypes->find(type) != sensitiveTypes->end();
}
bool FunctionAnalysis::isUnderlyingTypeStruct(Type *type) {
  if (type->isStructTy()) { return true; }
  if (type->isPointerTy()) { return isUnderlyingTypeStruct(type->getPointerElementType()); }
  if (type->isArrayTy()) { return isUnderlyingTypeStruct(type->getArrayElementType()); }
  return false;
}
FunctionAnalysis::FunctionAnalysis(Function &f, set<Type*> &sensitivetypes, Container_t &globals, Module &m) 
  : globals(globals), F(&f), M(&m),sensitiveTypes(&sensitivetypes) {
    // intialize container
    for (inst_iterator It = inst_begin(F), Ie = inst_end(F); It != Ie; ++It) {
      Instruction *I = &*It;
      locals.insert(pair<Value*, Protection>(I, NEVER));
    }

    for (Function::arg_iterator It = F->arg_begin();
        It != F->arg_end();
        ++It) 
    {
      Value *I = &*It;
      //errs() << "inserting arg: "; I->dump();
      locals.insert(pair<Value*, Protection>(I, NEVER));
    }

    for (Container_t::iterator it = globals.begin();
        it != globals.end();
        ++it)
    {
      if (isProtectedStruct(it->first)) {
        it->second = ALWAYS;
      }
    }

    //errs() << "Analyzing: " << F->getName() << "\n";
    analyze();
    //errs() << "done.\n";
  }
Protection FunctionAnalysis::getProtection(Value* I) {
  if (isa<ConstantExpr>(I) && isa<Operator>(I)) {
    if (Operator *op = cast<Operator>(I)) { 
      switch (op->getOpcode()) {
        case Instruction::BitCast:
        case Instruction::GetElementPtr:
        case Instruction::IntToPtr:
        case Instruction::PtrToInt:
          return getProtection(op->getOperand(0));
        case Instruction::Xor:
          {
            // could be Xor 2 variables, but
            // shouldnt that be an instruction?
            // most likely one operand is a variable
            // and the other is a constant
            // TODO: not 100% sure this is correct
            Protection p0 = getProtection(op->getOperand(0));
            Protection p1 = getProtection(op->getOperand(1));
            if (p0 == ALWAYS || p1 == ALWAYS) {
              return ALWAYS;
            } else if (p0 == CONST && p1 == CONST) {
              return CONST;
            } else {
              // CONST & NEVER = ????
              // NEVER i guess?
              return NEVER;
            }
          }
        default:
          errs() << "in module: " << M->getModuleIdentifier() << "\n";
          errs() << "in function: " << F->getName() << "\n";
          errs() << "opcode: " << op->getOpcode() << "\n";
          op->dump();
          llvm_unreachable("unhandled op code in getProtection");
      }
    }
  }
  // we dont trust external functions for now
  if (isa<Function>(I)) {
    return NEVER;
  }
  //if (Function* F = dyn_cast<Function>(I)) {
  //  if (F->isDeclaration()) {
  //    return NEVER;
  //  }
  //}
  Container_t::iterator it;
  if (isa<GlobalVariable>(I) || isa<GlobalValue>(I)) {
    //errs() << "its a global\n";
    it = globals.find(I);
  } else if (isa<ConstantInt>(I)) { 
    return CONST;
  } else if (isa<UndefValue>(I)) { 
    return CONST;
  } else if (isa<ConstantFP>(I)) { 
    return CONST;
  } else if (isa<ConstantPointerNull>(I)) { 
    return CONST;
  } else {
    //errs() << "its a local\n";
    it = locals.find(I);
  }
  if (it == globals.end() || it == locals.end()) {
    I->dump();
    errs() << "when analyzing function: " << F->getName() << "\n"; 
    errs() << "in module: " << M->getModuleIdentifier() << "\n"; 
    llvm_unreachable("couldn't find protection status");
  }
  return it->second;
}
void FunctionAnalysis::dump() {
  errs() << "START " << F->getName() << "\n";
  for (inst_iterator It = inst_begin(F), Ie = inst_end(F); It != Ie; ++It) {
    Instruction *I = &*It;
    Container_t::iterator ci = locals.find(I);
    errs() << "\t" << getString(ci->second) << "\t"; ci->first->dump();
  }
  errs() << "END " << F->getName() << "\n";
}
void ModuleAnalysis::getSensitiveTypes() {
  for (Module::global_iterator I = M.global_begin(), E = M.global_end();
      I != E;
      ++I) {
    if (I->getName() == "llvm.global.annotations") {
      Value *Op0 = I->getOperand(0);
      ConstantArray *arr = (ConstantArray*)(Op0);
      for (unsigned int i = 0; i < arr->getNumOperands(); ++i) {
        ConstantStruct *annoStruct = (ConstantStruct*)(arr->getOperand(i));
        Constant* cast = annoStruct->getOperand(0);
        Value* val = cast->getOperand(0);
        Value *ann = annoStruct->getOperand(1);
        Operator *op;
        Constant *c;
        ConstantDataArray *da;
        if (isa<Operator>(ann) && 
            (op = dyn_cast<Operator>(ann)) &&
            (op->getOpcode() == Instruction::GetElementPtr) &&
            (c = dyn_cast<Constant>(op->getOperand(0))) &&
            (da = dyn_cast<ConstantDataArray>(c->getOperand(0)))) 
        {
          StringRef sr = da->getAsCString();
          if (sr.equals("sensitive")) {
            if (val->getType()->isPointerTy()) {
              Type* sType = val->getType()->getPointerElementType();
              sensitiveTypes.insert(sType);
            }
            sensitiveTypes.insert(val->getType());
          } else {
            errs() << "in module: " << M.getModuleIdentifier() << "\n";
            errs() << sr << "\n";
            llvm_unreachable("unknown annotation");
          }

        } else {
          errs() << "in module: " << M.getModuleIdentifier() << "\n";
          annoStruct->dump();
          llvm_unreachable("unable to parse annotation");
        }
        //Constant* loc = annoStruct->getOperand(2);
        //errs() << "Annotated variable: " << val->getName();
        //errs() << " : "; val->getType()->dump();
        //errs() << "Annotated as: ";
        //ann->dump();
        //errs() << "In file: ";
        //loc->dump();
        // If type A is sensitive:
        // 1) if A is a array, the elements of A are sensitive
        // 2) if A is a struct, the fields of A are sensitive
        // (this code doesn't actually implement this)
      }
    }
  }
  //errs() << "This module's sensitive types are: \n";
  //set<Type*>::iterator it;
  //for (it = sensitiveTypes.begin(); it != sensitiveTypes.end(); ++it) {
  //  (*it)->dump();
  //  //printToErrs(*it);
  //  errs() << ", ";
  //}
  //errs() << "\n";
}
ModuleAnalysis::ModuleAnalysis(Module &m) : M(m) {
  getSensitiveTypes();
  // get global variables
  for (Module::global_iterator it = M.global_begin();
      it != M.global_end();
      ++it)
  {
    if (GlobalValue *GV = dyn_cast<GlobalValue>(&*it)) {
      globals.insert(pair<Value*, Protection>(GV, NEVER)); 
    }
  }

  for (Module::iterator It = M.begin(), Ie = M.end(); It != Ie; ++It) {
    Function &F = *It;
    if (!F.isDeclaration() && !F.getName().startswith("llvm.") &&
        !F.getName().startswith("__llvm__")) {
      //errs() << "Analyzing function: " << F.getName() << "\n";
      FunctionAnalysis fa(F, sensitiveTypes, globals, M);
      FnContainer.push_back(pair<Function*, FunctionAnalysis>(&F, fa));
      //fa.dump();
    }
  }
}
FunctionAnalysis &ModuleAnalysis::getFunctionAnalysis(Function &F) {
  for (FnContainer_t::iterator it = FnContainer.begin();
      it != FnContainer.end();
      ++it) 
  {
    if (it->first == &F) {
      return it->second;
    }
  }
  F.dump();
  llvm_unreachable("function not previously analyzed");
}

#endif
