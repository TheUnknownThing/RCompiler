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

inline void PassManager::run(ir::Module &module, std::ostream &os) {
  PhiElimination phiElimination(cfg);
  phiElimination.run(&module);

  InstructionSelection instSelect;
  instSelect.generate(module);

  RegAlloc regAlloc;
  regAlloc.allocate(instSelect.functions());

  PrologueEpiloguePass framePass;
  framePass.run(instSelect.functions());

  AsmEmitter asmEmitter;
  asmEmitter.emit(instSelect.functions(), os);
}

} // namespace rc::backend
