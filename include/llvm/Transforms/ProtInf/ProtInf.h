#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include <vector>
#include <set>

using namespace llvm;
using namespace std;

class ModuleInf {
  vector<pair<Value*, Value*> > equivalences;
  set<Value*> constrain_safe;
  set<Value*> constrain_unsafe;

  private:
  vector<tuple<Type*, Type*, Type*> > typeAliases;
  set<Type*> sensitiveTypes;
  Module &M;
  void getSensitiveTypes();
  vector<pair<StringRef, Type*>> getAnnotations();
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
