#ifndef __USEZ3__
#define __USEZ3__

#include <vector>
#include <set>
#include <z3++.h>

#include "llvm/IR/Value.h"
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

using namespace std;
using namespace llvm;

class ConstraintSolver {
  public:
  vector<Value*> solveConstraints( vector<pair<Value*, Value*> > &equivalences,
                                          vector<Value*> &constrain_safe,                            
                                          vector<Value*> &constrain_unsafe);
};

#endif
