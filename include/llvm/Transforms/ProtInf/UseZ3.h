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
using namespace z3;

class ConstraintSolver {
  public:
  ConstraintSolver( vector<pair<Value*, Value*> > &equivalences,
                                          set<Value*> &constrain_safe,                            
                                          set<Value*> &constrain_unsafe)
    : equiv(equivalences), constrain_s(constrain_safe), constrain_u(constrain_unsafe)  {}
  vector<Value*> solveConstraints(); 
  private:
  map<Value*, string> names;
  vector<pair<Value*, Value*> > &equiv;
  set<Value*> &constrain_s;                            
  set<Value*> &constrain_u;
  void print_err(expr_vector unsat_core);
  Value *getValue(string name);
};

#endif
