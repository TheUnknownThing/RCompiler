#include "backend/pass_manager.hpp"

namespace rc::backend {

void PassManager::run(ir::Module &module, std::ostream &os) {
  PhiElimination phi_elimination(cfg);
  phi_elimination.run(&module);

  InstructionSelection inst_select;
  inst_select.generate(module);

  RegAlloc reg_alloc;
  reg_alloc.allocate(inst_select.functions());

  PrologueEpiloguePass frame_pass;
  frame_pass.run(inst_select.functions());

  AsmEmitter asm_emitter;
  asm_emitter.emit(inst_select.functions(), os);
}

} // namespace rc::backend
