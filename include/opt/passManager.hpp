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


} // namespace rc::opt
