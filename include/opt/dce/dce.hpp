#pragma once

#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"

#include "opt/utils/cfg_pretty_print.hpp"

#include <unordered_set>
#include <vector>

namespace rc::opt {

class DeadCodeElimVisitor {
public:
  void run(ir::Module &module);

private:
  void trim_after_terminator(ir::BasicBlock &bb);
  void fold_constant_conditional_branches(ir::Function &function);
  std::unordered_set<ir::BasicBlock *> compute_reachable(ir::Function &function);
  void squash_unreachable_blocks(
      ir::Function &function,
      const std::unordered_set<ir::BasicBlock *> &reachable);
  void rebuild_predecessors(ir::Function &function);
  void remove_undef_phi_incoming_blocks(ir::Function &function);
};

} // namespace rc::opt
