#pragma once

#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"
#include <cstddef>

#include "opt/base/base_visitor.hpp"
#include "opt/utils/cfg_pretty_print.hpp"
#include "opt/utils/ir_utils.hpp"

#include "utils/logger.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rc::opt {

class SimplifyCFG {
public:
  void run(ir::Module *module);

  void simplify_cfg(ir::Function *func);

private:
  void remove_phi_incoming(ir::Function *func,
                         std::shared_ptr<ir::BasicBlock> old_bb);
  void replace_phi_incoming(ir::Function *func,
                          std::shared_ptr<ir::BasicBlock> old_bb,
                          std::shared_ptr<ir::BasicBlock> new_bb);
  void merge_blocks(ir::Function *func, std::shared_ptr<ir::BasicBlock> from,
                   std::shared_ptr<ir::BasicBlock> to);
  bool try_fold_trivial_phi(ir::BasicBlock &bb,
                         const std::shared_ptr<ir::PhiInst> &phi);
  bool fold_trivial_phis_in_block(ir::BasicBlock &bb);
  void rebuild_predecessors(ir::Function &function);
};

} // namespace rc::opt