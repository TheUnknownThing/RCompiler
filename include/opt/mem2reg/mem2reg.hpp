#pragma once

#include <cstddef>
#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "opt/base/baseVisitor.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"
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

  void removeUnusedAllocas(ir::Function &function);
  void replaceUseWithValue(ir::Function &function);

  void findDominators(ir::Function &function);
  void findIDom(ir::Function &function);
  void findDomFrontiers(ir::Function &function);

  void mem2reg(ir::Function &function);
  void placePhiNodes(ir::BasicBlock &bb, ir::AllocaInst *alloca);
  void rename(ir::BasicBlock &bb);
  void removeDeadInstructions(ir::Function &function);

private:
  std::unordered_map<const ir::BasicBlock *,
                     std::unordered_set<ir::BasicBlock *>>
      dominators_;
  std::unordered_map<ir::BasicBlock *, ir::BasicBlock *> idom_;
  std::unordered_map<const ir::BasicBlock *,
                     std::unordered_set<ir::BasicBlock *>>
      dominanceFrontiers_;

  std::unordered_map<const ir::AllocaInst *, std::vector<ir::Value *>>
      renameStacks_;
  std::unordered_map<const ir::BasicBlock *,
                     std::unordered_map<const ir::AllocaInst *, ir::PhiInst *>>
      phiNodes_; // this indicates the phi nodes placed for each alloca

  std::unordered_set<ir::Instruction *> toRemove_;
  std::unordered_set<const ir::AllocaInst *> promotableAllocas_;
  std::vector<std::shared_ptr<ir::UndefValue>> undefValues_;
};











} // namespace rc::opt