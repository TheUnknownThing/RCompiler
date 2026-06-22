#pragma once

#include <cstddef>
#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"

#include "opt/analysis/dominator_tree.hpp"
#include "opt/base/base_visitor.hpp"
#include "opt/utils/cfg_pretty_print.hpp"
#include "opt/utils/ir_utils.hpp"

#include "utils/logger.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rc::opt {
class Mem2RegVisitor {
public:
  virtual ~Mem2RegVisitor() = default;

  void run(ir::Module &module);

  void remove_unused_allocas(ir::Function &function);
  void replace_use_with_value(ir::Function &function);

  void mem2reg(ir::Function &function);
  void place_phi_nodes(ir::BasicBlock &bb, ir::AllocaInst *alloca);
  void rename(ir::BasicBlock &bb);
  void remove_dead_instructions(ir::Function &function);

private:
  DominatorTree dt_;

  std::unordered_map<const ir::AllocaInst *, std::vector<ir::Value *>>
      rename_stacks_;
  std::unordered_map<const ir::BasicBlock *,
                     std::unordered_map<const ir::AllocaInst *, ir::PhiInst *>>
      phi_nodes_; // this indicates the phi nodes placed for each alloca

  std::unordered_set<ir::Instruction *> to_remove_;
  std::unordered_set<const ir::AllocaInst *> promotable_allocas_;
  std::vector<std::shared_ptr<ir::UndefValue>> undef_values_;
};

} // namespace rc::opt
