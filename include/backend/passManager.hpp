#pragma once

#include "opt/cfg/cfg.hpp"
#include "passes/phiElimination.hpp"
#include "passes/instSelect.hpp"
#include "passes/pseudoAsmEmitter.hpp"

#include <iostream>

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

  InstructionSelection instSelect;
  instSelect.generate(module);

  PseudoAsmEmitter pseudoEmitter;
  pseudoEmitter.emit(instSelect.functions(), std::cout);
}

} // namespace rc::backend