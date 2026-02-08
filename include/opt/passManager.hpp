#pragma once

#include "ir/instructions/topLevel.hpp"

#include "opt/cfg/cfg.hpp"
#include "opt/dce/dce.hpp"
#include "opt/functionInline/functionInline.hpp"
#include "opt/instCombine/instCombine.hpp"
#include "opt/mem2reg/mem2reg.hpp"
#include "opt/sccp/context.hpp"
#include "opt/sccp/sccp.hpp"
#include "opt/simplifyCFG/simplifyCFG.hpp"

namespace rc::opt {

class PassManager {
public:
  explicit PassManager(ConstantContext &constCtx) : constCtx_(constCtx) {}

  void run(ir::Module &module);

private:
  void revisitCFG(ir::Module &module) {
    CFGVisitor cfg;
    cfg.run(module);
  }

  ConstantContext &constCtx_;
};

inline void PassManager::run(ir::Module &module) {
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
