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
#include "llvm/Support/FileSystem.h"
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
#include "llvm/Transforms/ProtInf/ProtInf.h"
#include "llvm/Transforms/ProtInf/UseZ3.h"

using namespace std;
using namespace llvm;

void ModuleInf::dumpModule() {
  std::error_code ec;
  StringRef name = "a";
  std::string mname = name.str() + "_analyze.ll";
  raw_fd_ostream fout(StringRef(mname), ec, sys::fs::F_None);
  M.print(fout, NULL);
  fout.close();
}

void ModuleInf::dumpConstraints() {
  std::error_code ec;
  raw_fd_ostream z3_input(StringRef("z3_input2"), ec, sys::fs::F_None);

  set<Value*> declared;

  for (auto& it : equivalences) {
    if (declared.find(it.first) == declared.end()) {
      declared.insert(it.first);
      z3_input << "(declare-const b" << it.first << " Bool)\n";
    }
    if (declared.find(it.second) == declared.end()) {
      declared.insert(it.second);
      z3_input << "(declare-const b" << it.second << " Bool)\n";
    }
    z3_input << "(assert (= b" << it.first << " b" << it.second << "))\n";
  }

  for (auto& it : constrain_safe) {
    if (declared.find(it) == declared.end()) {
      declared.insert(it);
      z3_input << "(declare-const b" << it << " Bool)\n";
    }
    z3_input << "(assert (= b" << it << " true))\n";
  }

  for (auto& it : constrain_unsafe) {
    if (declared.find(it) == declared.end()) {
      declared.insert(it);
      z3_input << "(declare-const b" << it << " Bool)\n";
    }
    z3_input << "(assert (= b" << it << " false))\n";
  }

  z3_input << "(check-sat)\n";
  z3_input << "(get-model)\n";

}

bool ModuleInf::isProtectedStruct(Value* val) {
  Type* type = val->getType();
  if (isSensitive(type) && isUnderlyingTypeStruct(type)) {
    return true;
  }
  return false;
}

void ModuleInf::equate(Value *v0, Value *v1) {
  // TODO we might need to handle bitcasts, etc
  equivalences.push_back(pair<Value*, Value*>(v0, v1));
}

void ModuleInf::analyzeFunction(Function &F) {
  for (inst_iterator It = inst_begin(F), Ie = inst_end(F); It != Ie; ++It) {
    Instruction *I = &*It;
    if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
      Value *ptrOp = SI->getPointerOperand();
      Value *valOp = SI->getValueOperand();
      equate(ptrOp, valOp);
      if (isProtectedStruct(ptrOp)) {
        constrain_safe.push_back(ptrOp);
        constrain_unsafe.push_back(valOp);
      }
    } else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
      Value *ptrOp = LI->getPointerOperand();
      equate(LI, ptrOp);
      if (isProtectedStruct(ptrOp)) {
        constrain_safe.push_back(ptrOp);
        constrain_unsafe.push_back(LI);
      }
    } else if (ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
      if (Value *rv = dyn_cast<Value>(RI)) {
        equate(rv, &F);
      }
    } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
      Function *Fn = CI->getCalledFunction();
      if (!Fn) {
        // todo function pointers
      } else {
        equate(CI, Fn);
        if (Fn->isDeclaration()) {
          // for now, let's try making all libraries unprotected
          for (auto& it : CI->arg_operands()) {
            constrain_unsafe.push_back(it);
          }
        }
        auto it = Fn->arg_begin();
        for (size_t i = 0; i < CI->getNumArgOperands(); ++i) 
        {
          Value *arg = CI->getArgOperand(i);
          equate(arg, it);
          ++it;
        }
      }
      //} else if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
    } else if (isa<AllocaInst>(I)) {
    // this variable is allocated on the stack which is safe
    } else if (isa<BranchInst>(I)) {
    //} else if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
    } else if (PHINode *Phi = dyn_cast<PHINode>(I)) {
      for (auto& i : Phi->operands()) {
        equate(Phi, i);
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
            //protect2OpInst(I);
            equate(I, I->getOperand(0));
            equate(I, I->getOperand(1));
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
            //protect1OpInst(I);
            equate(I, I->getOperand(0));
            break;
          }
        case Instruction::Select: 
          {
            //protect3OpInst(I);
            equate(I, I->getOperand(0));
            equate(I, I->getOperand(1));
            equate(I, I->getOperand(2));
            break;
          }
        case Instruction::Unreachable:
        case Instruction::Switch:
          break;
        default:
          errs() << "in module: " << M.getModuleIdentifier() << "\n";
          errs() << "in function: " << F.getName() << "\n";
          I->dump();
          llvm_unreachable("unhandled instruction type");
      }
    }
  }
}
bool ModuleInf::isSensitive(Type* type) {
  return sensitiveTypes.find(type) != sensitiveTypes.end();
}
bool ModuleInf::isUnderlyingTypeStruct(Type *type) {
  if (type->isStructTy()) { return true; }
  if (type->isPointerTy()) { return isUnderlyingTypeStruct(type->getPointerElementType()); }
  if (type->isArrayTy()) { return isUnderlyingTypeStruct(type->getArrayElementType()); }
  return false;
}
void ModuleInf::getSensitiveTypes() {
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
      }
    }
  }
}
ModuleInf::ModuleInf(Module &m) : M(m) {
  getSensitiveTypes();
  for (Module::iterator It = M.begin(), Ie = M.end(); It != Ie; ++It) {
    Function &F = *It;
    if (!F.isDeclaration() && !F.getName().startswith("llvm.") &&
        !F.getName().startswith("__llvm__")) {
      //errs() << "Analyzing function: " << F.getName() << "\n";
      analyzeFunction(F);
      //fa.dump();
    }
  }
  ConstraintSolver cs;
  cs.solveConstraints(equivalences, constrain_safe, constrain_unsafe);
}

