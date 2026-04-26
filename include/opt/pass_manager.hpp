#pragma once

#include "ir/instructions/top_level.hpp"

#include "opt/cfg/cfg.hpp"
#include "opt/dce/dce.hpp"
#include "opt/function_inline/function_inline.hpp"
#include "opt/inst_combine/inst_combine.hpp"
#include "opt/mem2reg/mem2reg.hpp"
#include "opt/sccp/context.hpp"
#include "opt/sccp/sccp.hpp"
#include "opt/simplify_cfg/simplify_cfg.hpp"

namespace rc::opt {

class PassManager {
public:
  explicit PassManager(ConstantContext &const_ctx) : const_ctx_(const_ctx) {}

  void run(ir::Module &module);

private:
  void revisit_cfg(ir::Module &module) {
    CFGVisitor cfg;
    cfg.run(module);
  }

  ConstantContext &const_ctx_;
};

} // namespace rc::opt
