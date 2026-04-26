#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"

#include "opt/utils/cfgPrettyPrint.hpp"

#include <unordered_set>
#include <vector>

namespace rc::opt {

class DeadCodeElimVisitor {
public:
  void run(ir::Module &module);

private:
  void trimAfterTerminator(ir::BasicBlock &bb);
  void foldConstantConditionalBranches(ir::Function &function);
  std::unordered_set<ir::BasicBlock *> computeReachable(ir::Function &function);
  void squashUnreachableBlocks(
      ir::Function &function,
      const std::unordered_set<ir::BasicBlock *> &reachable);
  void rebuildPredecessors(ir::Function &function);
  void removeUndefPhiIncomingBlocks(ir::Function &function);
};








} // namespace rc::opt
