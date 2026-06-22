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

  LoadForwardingPass load_forward;
  load_forward.run(module);

  SCCPVisitor sccp(&const_ctx_);
  sccp.run(module);

  inst_combine.run(module);

  load_forward.run(module);

  dce.run(module);

  SimplifyCFG simplify_cfg;
  simplify_cfg.run(&module);

  revisit_cfg(module);

  // Hoist loop-invariant pure computation into preheaders before switch
  // recovery — recovery's pattern match assumes canonical compare-and-branch
  // test blocks, and LICM moves invariants out of those without disturbing the
  // ladder shape.
  LICMPass licm;
  licm.run(module);

  revisit_cfg(module);

  inst_combine.run(module);
  dce.run(module);

  revisit_cfg(module);

  // Collapse if/else-if ladders into switches once the CFG is canonical
  // (constants folded, blocks merged), then clean up the now-dead test blocks.
  SwitchRecovery switch_recovery;
  switch_recovery.run(module);

  revisit_cfg(module);

  dce.run(module);

  revisit_cfg(module);
}

} // namespace rc::opt
