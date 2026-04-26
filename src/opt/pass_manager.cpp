#include "opt/pass_manager.hpp"

namespace rc::opt {

void PassManager::run(ir::Module &module) {
  revisit_cfg(module);

  DeadCodeElimVisitor dce;
  dce.run(module);

  revisit_cfg(module);

  Mem2RegVisitor mem2reg;
  mem2reg.run(module);

  FunctionInline func_inline;
  func_inline.run(module);

  InstCombinePass inst_combine(&const_ctx_);
  inst_combine.run(module);

  revisit_cfg(module);

  SCCPVisitor sccp(&const_ctx_);
  sccp.run(module);

  inst_combine.run(module);

  dce.run(module);

  SimplifyCFG simplify_cfg;
  simplify_cfg.run(&module);

  revisit_cfg(module);
}

} // namespace rc::opt
