#ifndef __DATACONF_INC__
#define __DATACONF_INC__
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace DataConf {

  enum Protection {NEVER=0, ALWAYS, MAYBE, CONST};

  void dumpModule(Module &M);
}
#endif
