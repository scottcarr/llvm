#include <vector>
#include <set>
#include <map>
#include <z3++.h>
#include <iostream>

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
#include "llvm/Transforms/ProtInf/UseZ3.h"
#include "llvm/Support/FileSystem.h"

using namespace std;
using namespace z3;
using namespace llvm;

vector<Value*> ConstraintSolver::solveConstraints( 
    vector<pair<Value*, Value*> > &equivalences,
    vector<Value*> &constrain_safe,                            
    vector<Value*> &constrain_unsafe) 
{
  //set<Value*> declared;
  vector<Value*> result;
  stringstream ss;
  int i = 0;
  map<Value*, string> names;

  std::error_code ec;
  raw_fd_ostream z3_input(StringRef("z3_input"), ec, sys::fs::F_None);

  config cfg;
  //cfg.set("auto-config", true);
  cfg.set("unsat_core", true);
  //cout << cfg << "\n";
  context c(cfg);
  //context c;
  solver s(c);
  //expr bc1 = c.bool_const("x");
  //expr bc2 = c.bool_const("y");
  //expr e = bc1 == bc2;
  //s.add(e);
    

  for (auto it : equivalences) {
    if (names.find(it.first) == names.end()) {
      ss.str("");
      ss << "b" << i++;
      names.insert(pair<Value*, string> (it.first, ss.str()));
      z3_input << "(declare-const b" << it.first << " Bool)\n";
    }
    if (names.find(it.second) == names.end()) {
      ss.str("");
      ss << "b" << i++;
      names.insert(pair<Value*, string> (it.second, ss.str()));
      z3_input << "(declare-const b" << it.second << " Bool)\n";
    }
  }
  for (auto it : constrain_safe) {
    if (names.find(it) == names.end()) {
      ss.str("");
      ss << "b" << i++;
      names.insert(pair<Value*, string> (it, ss.str()));
      z3_input << "(declare-const b" << it << " Bool)\n";
    }
  }
  for (auto it : constrain_unsafe) {
    if (names.find(it) == names.end()) {
      ss.str("");
      ss << "b" << i++;
      names.insert(pair<Value*, string> (it, ss.str()));
      z3_input << "(declare-const b" << it << " Bool)\n";
    }
  }

  for (auto& it : equivalences) {
    auto name0 = names.find(it.first)->second;
    auto name1 = names.find(it.second)->second;
    expr bc1 = c.bool_const(name0.c_str());
    expr bc2 = c.bool_const(name1.c_str());
    expr eqv = bc1 == bc2;
    string label = name0 + "==" + name1;
    s.add(eqv, label.c_str());
    z3_input << "(assert (= b" << it.first << " b" << it.second << "))\n";
  }

  for (auto& it : constrain_unsafe) {
    auto name = names.find(it)->second.c_str();
    expr b = c.bool_const(name);
    expr constr = b == c.bool_val(false);
    s.add(constr);
    z3_input << "(assert (= b" << it << " false))\n";
  }

  for (auto& it : constrain_safe) {
    auto name = names.find(it)->second.c_str();
    expr b = c.bool_const(name);
    expr constr = b == c.bool_val(true);
    s.add(constr);
    z3_input << "(assert (= b" << it << " true))\n";
  }

  //for (auto& it : constrain_safe) {
  //  expr bc = c.bool_const(it->getValueName()->first().str().c_str());
  //  expr constr = bc == c.bool_val(true);
  //  s.add(constr);
  //}
  //for (auto& it : constrain_unsafe) {
  //  expr bc = c.bool_const(it->getValueName()->first().str().c_str());
  //  expr constr = bc == c.bool_val(false);
  //  s.add(constr);
  //}

  cout << s << "\n";
  switch (s.check()) {
    case unsat: 
      {
      errs() << "unsat\n";
      expr_vector core = s.unsat_core();
      errs() << "unsat core: " << core << "\n";
      errs() << "size: " << core.size() << "\n";
      for (size_t i = 0; i < core.size(); ++i) {
        cout << "\t" << core[i] << "\n";
      }
      }
      break;
    case sat: 
      {
      errs() << "sat\n";
      model m = s.get_model();
      cout << "model :" << m << "\n";
      break;
      }
    case unknown: 
      errs() << "unknonw\n";
  }
  return result;
}
