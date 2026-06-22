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

  AsmEmitter asm_emitter(/*register_size=*/4);
  asm_emitter.emit(inst_select.functions(), module.constants(), os);
}

void PassManager::run_rv64(ir::Module &module, std::ostream &os) {
  PhiElimination phi_elimination(cfg);
  phi_elimination.run(&module);

  InstructionSelection inst_select(/*register_size=*/8);
  inst_select.generate(module);

  RegAlloc reg_alloc(/*spill_slot_size=*/8, InstOpcode::LD, InstOpcode::SD);
  reg_alloc.allocate(inst_select.functions());

  PrologueEpiloguePass frame_pass(/*saved_reg_size=*/8, InstOpcode::SD,
                                  InstOpcode::LD);
  frame_pass.run(inst_select.functions());

  AsmEmitter asm_emitter(/*register_size=*/8);
  asm_emitter.emit(inst_select.functions(), module.constants(), os);
}

} // namespace rc::backend
