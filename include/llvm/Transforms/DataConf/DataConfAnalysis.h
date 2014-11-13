#ifndef __DATACONFANALYSIS_INC_
#define __DATACONFANALYSIS_INC_
#include "llvm/IR/Value.h"
#include "llvm/Transforms/DataConf/DataConf.h"

#include <map>
#include <set>

using namespace llvm;
using namespace std;
using namespace DataConf;


namespace DataConfAnalysis {
  typedef map<Value*, Protection> Container_t;
  string getString(Protection p);
  class FunctionAnalysis {
    private:
      bool isChanged = true;
      Container_t locals;
      Container_t globals;
      Function *F;
      Module *M;
      set<Type*> *sensitiveTypes;
      bool isProtectedStruct(Value* val);
      void update(Value* I, Protection status);
      void resetIsChanged() { isChanged = false; }
      bool getIsChanged() { return isChanged; }
      void protect3OpInst(Instruction *I);
      void protect2OpInst(Instruction *I);
      void protect1OpInst(Instruction *I);
      void analyze();
    public:
      bool isSensitive(Type* type);
      bool isUnderlyingTypeStruct(Type *type);
      FunctionAnalysis(Function &f, 
                       set<Type*> &sensitivetypes, 
                       Container_t &globals, 
                       Module &m);
      Protection getProtection(Value* I);
      void dump();
  };
  class ModuleAnalysis {
    private:
      typedef vector<pair<Function*, FunctionAnalysis> > FnContainer_t;
      FnContainer_t FnContainer;
      set<Type*> sensitiveTypes;
      Container_t globals;
      Module &M;
      void getSensitiveTypes();
    public: 
      ModuleAnalysis(Module &m);
      FunctionAnalysis &getFunctionAnalysis(Function &F);
  };
}
#endif
