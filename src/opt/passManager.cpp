#include "opt/passManager.hpp"

namespace rc::opt {

void PassManager::run(ir::Module &module) {
  revisitCFG(module);

  DeadCodeElimVisitor dce;
  dce.run(module);

  revisitCFG(module);

  Mem2RegVisitor mem2reg;
  mem2reg.run(module);

  FunctionInline funcInline;
  funcInline.run(module);

  InstCombinePass instCombine(&constCtx_);
  instCombine.run(module);

  revisitCFG(module);

  SCCPVisitor sccp(&constCtx_);
  sccp.run(module);

  instCombine.run(module);

  dce.run(module);

  SimplifyCFG simplifyCFG;
  simplifyCFG.run(&module);

  revisitCFG(module);
}

} // namespace rc::opt
