#pragma once

#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"
#include "opt/cfg/cfg.hpp"
#include "opt/utils/cfg_pretty_print.hpp"

#include <unordered_set>

namespace rc::backend {

class PhiElimination {
public:
  PhiElimination(opt::CFGVisitor &cfg) : cfg(cfg) {}
  void run(ir::Module *module);

private:
  opt::CFGVisitor &cfg;

  void eliminate_phi_in_function(ir::Function *func);
  void eliminate_critical_edge(ir::Function *func);
  void replace_critical_edge(ir::Function *func, ir::BasicBlock *from,
                           ir::BasicBlock *to);
};

} // namespace rc::backend