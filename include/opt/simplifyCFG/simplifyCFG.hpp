#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"
#include <cstddef>

#include "opt/base/baseVisitor.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"

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
  void removePhiIncoming(ir::Function *func,
                         std::shared_ptr<ir::BasicBlock> oldBB);
  void replacePhiIncoming(ir::Function *func,
                          std::shared_ptr<ir::BasicBlock> oldBB,
                          std::shared_ptr<ir::BasicBlock> newBB);
  void mergeBlocks(ir::Function *func, std::shared_ptr<ir::BasicBlock> from,
                   std::shared_ptr<ir::BasicBlock> to);
};

inline void SimplifyCFG::run(ir::Module *module) {
  for (auto &function : module->functions()) {
    simplifyCFG(function.get());
  }
}

inline void SimplifyCFG::simplifyCFG(ir::Function *func) {
  std::unordered_set<ir::BasicBlock *> unreachable;
  if (func->blocks().empty()) {
    return;
  }

  // after DCE, the unrechable blocks are only with one UnreachableInst
  auto prevBlocks = func->blocks();
  for (const auto &bb : prevBlocks) {
    if (auto inst = bb->instructions().empty()
                        ? nullptr
                        : std::dynamic_pointer_cast<ir::UnreachableInst>(
                              bb->instructions().front())) {
      unreachable.insert(bb.get());
    }
  }

  for (const auto &bb : unreachable) {
    removePhiIncoming(func, bb->shared_from_this());
    func->eraseBlock(bb->shared_from_this());
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &bb : func->blocks()) {
      auto succs = utils::detail::successors(*bb);
      if (succs.size() == 1) {
        auto succ = succs.front();
        if (succ->predecessors().size() == 1 &&
            succ->predecessors().front() == bb.get()) {
          auto succBB = const_cast<ir::BasicBlock *>(succ)->shared_from_this();
          mergeBlocks(func, succBB, bb);
          changed = true;
          break;
        }
      }
    }
  }
}

inline void
SimplifyCFG::removePhiIncoming(ir::Function *func,
                               std::shared_ptr<ir::BasicBlock> oldBB) {
  for (const auto &bb : func->blocks()) {
    for (const auto &inst : bb->instructions()) {
      if (auto phi = std::dynamic_pointer_cast<ir::PhiInst>(inst)) {
        phi->removeIncomingBlock(oldBB.get());
      }
    }
  }
}

inline void
SimplifyCFG::replacePhiIncoming(ir::Function *func,
                                std::shared_ptr<ir::BasicBlock> oldBB,
                                std::shared_ptr<ir::BasicBlock> newBB) {
  for (const auto &bb : func->blocks()) {
    for (const auto &inst : bb->instructions()) {
      if (auto phi = std::dynamic_pointer_cast<ir::PhiInst>(inst)) {
        phi->replaceIncomingBlock(oldBB, newBB);
      }
    }
  }
}

inline void SimplifyCFG::mergeBlocks(ir::Function *func,
                                     std::shared_ptr<ir::BasicBlock> from,
                                     std::shared_ptr<ir::BasicBlock> to) {
  // Move instructions from 'from' to 'to'
  auto &fromInsts = from->instructions();
  auto &toInsts = to->instructions();

  auto inst = toInsts.empty() ? nullptr : toInsts.back();
  if (inst) {
    inst->dropAllReferences();
    to->eraseInstruction(inst);
  }

  if (!fromInsts.empty() && !toInsts.empty()) {
    fromInsts.back()->setNext(toInsts.front().get());
    toInsts.front()->setPrev(fromInsts.back().get());
  }

  toInsts.insert(toInsts.end(), fromInsts.begin(), fromInsts.end());
  fromInsts.clear();

  replacePhiIncoming(func, from, to);
  func->eraseBlock(from);
}

} // namespace rc::opt