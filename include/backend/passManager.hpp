#pragma once

#include "opt/cfg/cfg.hpp"
#include "passes/asmEmitter.hpp"
#include "passes/phiElimination.hpp"
#include "passes/instSelect.hpp"
#include "passes/prologueEpilogue.hpp"
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
