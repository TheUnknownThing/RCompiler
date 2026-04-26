#pragma once

#include "opt/cfg/cfg.hpp"
#include "passes/asm_emitter.hpp"
#include "passes/phi_elimination.hpp"
#include "passes/inst_select.hpp"
#include "passes/prologue_epilogue.hpp"
#include "passes/regalloc.hpp"

#include <iostream>

namespace rc::backend {

class PassManager {
public:
  void run(ir::Module &module, std::ostream &os);

private:
  opt::CFGVisitor cfg;
};


} // namespace rc::backend
