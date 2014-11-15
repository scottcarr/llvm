#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include <vector>
#include <set>

using namespace llvm;
using namespace std;

class ModuleInf {
  vector<pair<Value*, Value*> > equivalences;
  vector<Value*> constrain_safe;
  vector<Value*> constrain_unsafe;

  private:
  set<Type*> sensitiveTypes;
  Module &M;
  void getSensitiveTypes();
  void analyzeFunction(Function &F);
  bool isSensitive(Type *t);
  bool isUnderlyingTypeStruct(Type *t);
  void equate(Value *v0, Value* v1);
  bool isProtectedStruct(Value* val);

  public:
  ModuleInf(Module &M);
  void dumpModule(void);
  void dumpConstraints(void);
};
