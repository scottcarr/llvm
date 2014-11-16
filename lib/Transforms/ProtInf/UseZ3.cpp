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

void ConstraintSolver::print_err(expr_vector unsat_core) {
  stringstream ss;
  for (size_t i = 0; i < unsat_core.size(); ++i) {
    //cout << "\t" << unsat_core[i] << "\n";
    ss.str("");
    ss << unsat_core[i];
    size_t eqIndex = ss.str().find_first_of("==");
    string b0 = ss.str().substr(0, eqIndex);
    string b1 = ss.str().substr(eqIndex + 2, ss.str().size() - b0.size() - 2);
    //cout << b0 << " == " << b1 << "\n";
    Value *v0 = getValue(b0);
    Value *v1 = getValue(b1);

    if (constrain_s.find(v0) != constrain_s.end()) {
      errs() << "(safe)";
    }
    errs() << "\t";
    if (constrain_u.find(v0) != constrain_s.end()) {
      errs() << "(unsafe)";
    }
    errs() << "\t";
    v0->print(errs());
    errs() << "\n";
    if (constrain_s.find(v1) != constrain_s.end()) {
      errs() << "(safe)";
    }
    errs() << "\t";
    if (constrain_u.find(v1) != constrain_s.end()) {
      errs() << "(unsafe)";
    }
    errs() << "\t";
    v1->print(errs());
    errs() << "\n";
    errs() << "\n";
   
  }
}
Value *ConstraintSolver::getValue(string name) {
  for (auto it : names) {
    if (it.second == name) {
      return it.first;
    }
  }
  errs() << name << "\n";
  llvm_unreachable("name not found");
}

void ConstraintSolver::translateModel(context *c, model *m, map<Value*, bool> &results) {
  stringstream ss;
  for (size_t i = 0; i < m->num_consts(); ++i) {
    ss.str("");
    bool result;
    func_decl fd = m->get_const_decl(i);
    expr interp = m->get_const_interp(fd);
    ss << fd;
    if (ss.str().find("==")) {
      continue;
    }
    if (eq(interp, c->bool_val(false))) {
      result = false;
    } else {
      result = true;
    }
    for (auto& it : names) {
      if (it.second == fd.name().str()) {
        results[it.first] = result;
        continue;
      }
    }
    llvm_unreachable("should have found name");
  }
}

bool ConstraintSolver::trySolveConstraints(map<Value*, bool> &results)
{
  stringstream ss;
  int i = 0;

  std::error_code ec;
  raw_fd_ostream z3_input(StringRef("z3_input"), ec, sys::fs::F_None);

  config cfg;
  cfg.set("unsat_core", true);
  //cfg.set("model_validate", true);
  context c(cfg);
  solver s(c);

  for (auto it : equiv) {
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
  for (auto it : constrain_s) {
    if (names.find(it) == names.end()) {
      ss.str("");
      ss << "b" << i++;
      names.insert(pair<Value*, string> (it, ss.str()));
      z3_input << "(declare-const b" << it << " Bool)\n";
    }
  }
  for (auto it : constrain_u) {
    if (names.find(it) == names.end()) {
      ss.str("");
      ss << "b" << i++;
      names.insert(pair<Value*, string> (it, ss.str()));
      z3_input << "(declare-const b" << it << " Bool)\n";
    }
  }

  for (auto& it : equiv) {
    auto name0 = names.find(it.first)->second;
    auto name1 = names.find(it.second)->second;
    expr bc1 = c.bool_const(name0.c_str());
    expr bc2 = c.bool_const(name1.c_str());
    expr eqv = bc1 == bc2;
    string label = name0 + "==" + name1;
    s.add(eqv, label.c_str());
    z3_input << "(assert (= b" << it.first << " b" << it.second << "))\n";
  }

  for (auto& it : constrain_u) {
    auto name = names.find(it)->second.c_str();
    expr b = c.bool_const(name);
    expr constr = b == c.bool_val(false);
    s.add(constr);
    z3_input << "(assert (= b" << it << " false))\n";
  }

  for (auto& it : constrain_s) {
    auto name = names.find(it)->second.c_str();
    expr b = c.bool_const(name);
    expr constr = b == c.bool_val(true);
    s.add(constr);
    z3_input << "(assert (= b" << it << " true))\n";
  }

  switch (s.check()) {
    case sat: 
      {
      model m = s.get_model();
      errs() << "sat\n";
      translateModel(&c, &m, results);
      return true;
      }
    case unsat: 
      {
      errs() << "unsat\n";
      print_err(s.unsat_core());
      }
      return true;
    case unknown: 
      errs() << "unknonw\n";
      return false;
  }
}
