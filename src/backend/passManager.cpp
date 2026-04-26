#include "backend/passManager.hpp"

namespace rc::backend {

void PassManager::run(ir::Module &module, std::ostream &os) {
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
