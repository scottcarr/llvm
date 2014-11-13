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

  static void dumpModule(Module &M) {
    std::error_code ec;
    StringRef name = "a";
    std::string mname = name.str() + ".ll";
    //raw_fd_ostream fout(mname.c_str(), errorStr);
    raw_fd_ostream fout(StringRef(mname), ec, sys::fs::F_None);
    M.print(fout, NULL);
    fout.close();
  }
}
#endif
