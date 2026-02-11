#pragma once

#include "opt/cfg/cfg.hpp"
#include "passes/phiElimination.hpp"

namespace rc::backend {

class PassManager {
public:
  void run(ir::Module &module);

private:
  opt::CFGVisitor cfg;
};

inline void PassManager::run(ir::Module &module) {
  PhiElimination phiElimination(cfg);
  phiElimination.run(&module);
}

} // namespace rc::backend