#SET(FPIC "-fPIC")
#add_definitions(${FPIC})

add_llvm_loadable_module( LLVMProtInf
  ProtInf.cpp
  ProtInfPass.cpp
  UseZ3.cpp
  )

target_link_libraries(LLVMProtInf z3)

