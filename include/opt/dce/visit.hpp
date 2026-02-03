#pragma once

#include "ir/instructions/controlFlow.hpp"
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
};

inline void DeadCodeElimVisitor::run(ir::Module &module) {
  for (const auto &function : module.functions()) {
    for (const auto &bb : function->blocks()) {
      trimAfterTerminator(*bb);
    }

    foldConstantConditionalBranches(*function);

    auto reachable = computeReachable(*function);
    squashUnreachableBlocks(*function, reachable);

    rebuildPredecessors(*function);
  }
}

inline void DeadCodeElimVisitor::trimAfterTerminator(ir::BasicBlock &bb) {
  auto &instrs = bb.instructions();
  bool foundTerminator = false;

  for (auto it = instrs.begin(); it != instrs.end();) {
    auto *inst = it->get();
    if (!inst) {
      ++it;
      continue;
    }

    if (foundTerminator) {
      auto inst = std::static_pointer_cast<ir::Instruction>(*it);
      inst->dropAllReferences();

      auto *prev = inst->prev();
      auto *next = inst->next();
      if (prev) {
        prev->setNext(next);
      }
      if (next) {
        next->setPrev(prev);
      }
      it = instrs.erase(it);
      continue;
    }

    if (dynamic_cast<ir::BranchInst *>(inst) ||
        dynamic_cast<ir::ReturnInst *>(inst) ||
        dynamic_cast<ir::UnreachableInst *>(inst)) {
      foundTerminator = true;
    }

    ++it;
  }
}

inline void
DeadCodeElimVisitor::foldConstantConditionalBranches(ir::Function &function) {
  for (const auto &bbPtr : function.blocks()) {
    if (!bbPtr) {
      continue;
    }

    auto &bb = *bbPtr;
    auto &instrs = bb.instructions();
    if (instrs.empty()) {
      continue;
    }

    for (auto it = instrs.begin(); it != instrs.end(); ++it) {
      auto *br = dynamic_cast<ir::BranchInst *>(it->get());
      if (!br) {
        continue;
      }
      if (!br->isConditional()) {
        break;
      }

      auto *ci = dynamic_cast<ir::ConstantInt *>(br->cond().get());
      if (!ci) {
        break;
      }

      const bool takeTrue = (ci->value() != 0);
      auto chosen = takeTrue ? br->dest() : br->altDest();
      if (!chosen) {
        break;
      }

      // Replace the conditional branch with an unconditional one.
      auto oldInst = std::static_pointer_cast<ir::Instruction>(*it);
      oldInst->dropAllReferences();

      auto *prev = oldInst->prev();
      auto *next = oldInst->next();
      if (prev) {
        prev->setNext(next);
      }
      if (next) {
        next->setPrev(prev);
      }

      it = instrs.erase(it);

      auto newBr = std::make_shared<ir::BranchInst>(&bb, chosen);
      newBr->setPrev(prev);
      newBr->setNext(next);
      if (prev) {
        prev->setNext(newBr.get());
      }
      if (next) {
        next->setPrev(newBr.get());
      }

      instrs.insert(it, std::move(newBr));
      break;
    }
  }
}

inline std::unordered_set<ir::BasicBlock *>
DeadCodeElimVisitor::computeReachable(ir::Function &function) {
  std::unordered_set<ir::BasicBlock *> reachable;
  const auto &blocks = function.blocks();
  if (blocks.empty()) {
    return reachable;
  }

  std::vector<ir::BasicBlock *> worklist;
  worklist.push_back(blocks.front().get());
  reachable.insert(blocks.front().get());

  while (!worklist.empty()) {
    auto *bb = worklist.back();
    worklist.pop_back();

    for (auto *succ : utils::detail::successors(*bb)) {
      auto *succ_nonconst = const_cast<ir::BasicBlock *>(succ);
      if (reachable.insert(succ_nonconst).second) {
        worklist.push_back(succ_nonconst);
      }
    }
  }

  return reachable;
}

inline void DeadCodeElimVisitor::squashUnreachableBlocks(
    ir::Function &function,
    const std::unordered_set<ir::BasicBlock *> &reachable) {
  for (const auto &bb : function.blocks()) {
    if (reachable.count(bb.get())) {
      continue;
    }

    auto &instrs = bb->instructions();
    for (auto &inst : instrs) {
      if (inst) {
        std::static_pointer_cast<ir::Instruction>(inst)->dropAllReferences();
      }
    }
    instrs.clear();
    bb->append<ir::UnreachableInst>();
  }
}

inline void DeadCodeElimVisitor::rebuildPredecessors(ir::Function &function) {
  // Clear existing predecessor lists.
  for (const auto &bb : function.blocks()) {
    bb->clearPredecessors();
  }

  // Add predecessors based on branch terminators.
  for (const auto &bb : function.blocks()) {
    auto succs = utils::detail::successors(*bb);
    for (auto *succ : succs) {
      auto *succ_nonconst = const_cast<ir::BasicBlock *>(succ);
      succ_nonconst->addPredecessor(bb.get());
    }
  }
}

} // namespace rc::opt
