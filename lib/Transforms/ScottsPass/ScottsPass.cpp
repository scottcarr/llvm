#include <unordered_set>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;
using namespace std;

StringRef getPointeeString(Constant *c) {
    errs() << c->getNumOperands() << "\n";
    Constant* o0 = (Constant*)c->getOperand(0);
    Constant* o1 = (Constant*)c->getOperand(1);
    Constant* o2 = (Constant*)c->getOperand(2);

    ConstantDataSequential* o00 = (ConstantDataSequential*)o0->getOperand(0);
    return o00->getAsCString();
}


unordered_set<Type*> annotatedTypes;

class MyAnnotation{
    public:
    MyAnnotation(Value* val, Constant* annotation, Constant* filename, Constant* linenum, Module& m) :
        Val(val), Annotation(annotation), Filename(filename), Linenum(linenum), M(m) { }
    friend ostream& operator<<(ostream &o, const MyAnnotation& ma);
    void dump() {
        errs() << "Value: {";
        dumpValue(Val);
        errs() << "}\n";
        errs() << "Annotation: {";
        dumpConstant(Annotation);
        errs() << "}\n";
        errs() << "FileName: {";
        dumpConstant(Filename);
        errs() << "}\n";
        errs() << "Line #: ";
        Linenum->printAsOperand(errs(), false, &M);
        errs() << "\n";
    }
    void dumpConstant(Constant* c) {
        for (size_t i = 0; i < c->getNumOperands(); i++) {
            Constant *v = (Constant*)c->getOperand(i);
            if (i == 0) {
                ConstantDataSequential* s = (ConstantDataSequential*) v->getOperand(0);
                errs() << s->getAsCString();
            } else {
                errs() << ", ";
                v->printAsOperand(errs(), false, &M);
            }
        }
    }
    void dumpValue(Value* v) {
        errs() << v->getName() << ", ";
        v->getType()->print(errs());
    }
    Type* getType() {
        return Val->getType();
    }
    private:
    Value* Val;
    Constant* Annotation;
    Constant* Filename;
    Constant* Linenum;
    const Module& M;
};

namespace {
    struct ScottsPass: public ModulePass {
        static char ID;
        ScottsPass() : ModulePass(ID) {}
        virtual bool runOnModule(Module &M) {
            //errs() << "Hello: ";
            //errs().write_escaped(M.getModuleIdentifier()) << "\n";
            for (Module::global_iterator I = M.global_begin(), E = M.global_end();
                    I != E;
                    ++I) {
                if (I->getName() == "llvm.global.annotations") {
                    Value *Op0 = I->getOperand(0);
                    ConstantArray *arr = (ConstantArray*)(Op0);
                    for (size_t i = 0; i < arr->getNumOperands(); i++) {
                        ConstantStruct *annoStruct = (ConstantStruct*)(arr->getOperand(i));
                        Constant* cast = annoStruct->getOperand(0);
                        Value* val = cast->getOperand(0);
                        Constant* ann = annoStruct->getOperand(1);
                        Constant* file = annoStruct->getOperand(2);
                        Constant* num = annoStruct->getOperand(3);

                        MyAnnotation ma(val, ann, file, num, M);
                        ma.dump();

                        annotatedTypes.insert(ma.getType());
                    }
                }
            }
            errs() << "Annotated types: ";
            for (unordered_set<Type*>::iterator t = annotatedTypes.begin(); t != annotatedTypes.end(); t++) {
                (*t)->dump();
                errs() << " ";
            }
            errs() << "\n";
            return false;
        }
    };
}

char ScottsPass::ID = 0;
static RegisterPass<ScottsPass> X("ScottsPass", "Scott's pass", false, false);

