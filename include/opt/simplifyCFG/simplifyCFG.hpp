#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"
#include <cstddef>

#include "opt/base/baseVisitor.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"
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

  void simplifyCFG(ir::Function *func);

private:
  void removePhiIncoming(ir::Function *func,
                         std::shared_ptr<ir::BasicBlock> oldBB);
  void replacePhiIncoming(ir::Function *func,
                          std::shared_ptr<ir::BasicBlock> oldBB,
                          std::shared_ptr<ir::BasicBlock> newBB);
  void mergeBlocks(ir::Function *func, std::shared_ptr<ir::BasicBlock> from,
                   std::shared_ptr<ir::BasicBlock> to);
  bool tryFoldTrivialPhi(ir::BasicBlock &bb,
                         const std::shared_ptr<ir::PhiInst> &phi);
  bool foldTrivialPhisInBlock(ir::BasicBlock &bb);
  void rebuildPredecessors(ir::Function &function);
};









} // namespace rc::opt