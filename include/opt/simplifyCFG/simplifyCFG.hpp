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

  rebuildPredecessors(*func);

  bool changed = true;
  while (changed) {
    changed = false;

    for (auto &bb : func->blocks()) {
      changed |= foldTrivialPhisInBlock(*bb);
    }
    if (changed) {
      continue;
    }

    rebuildPredecessors(*func);
    for (auto &bb : func->blocks()) {
      auto succs = utils::detail::successors(*bb);
      if (succs.size() == 1) {
        auto succ = succs.front();
        if (succ->predecessors().size() == 1 &&
            succ->predecessors().front() == bb.get()) {
          auto succBB = succ->shared_from_this();
          mergeBlocks(func, succBB, bb);
          rebuildPredecessors(*func);
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
    auto &insts = bb->instructions();
    for (std::size_t i = 0; i < insts.size();) {
      auto phi = std::dynamic_pointer_cast<ir::PhiInst>(insts[i]);
      if (!phi) {
        ++i;
        continue;
      }

      phi->removeIncomingBlock(oldBB.get());
      if (tryFoldTrivialPhi(*bb, phi)) {
        continue;
      }

      ++i;
    }
  }
}

inline void
SimplifyCFG::replacePhiIncoming(ir::Function *func,
                                std::shared_ptr<ir::BasicBlock> oldBB,
                                std::shared_ptr<ir::BasicBlock> newBB) {
  for (const auto &bb : func->blocks()) {
    auto &insts = bb->instructions();
    for (std::size_t i = 0; i < insts.size();) {
      auto phi = std::dynamic_pointer_cast<ir::PhiInst>(insts[i]);
      if (!phi) {
        ++i;
        continue;
      }

      phi->replaceIncomingBlock(oldBB.get(), newBB.get());
      if (tryFoldTrivialPhi(*bb, phi)) {
        continue;
      }

      ++i;
    }
  }
}

inline void SimplifyCFG::mergeBlocks(ir::Function *func,
                                     std::shared_ptr<ir::BasicBlock> from,
                                     std::shared_ptr<ir::BasicBlock> to) {
  foldTrivialPhisInBlock(*from);

  // Move instructions from 'from' to 'to'
  auto &fromInsts = from->instructions();
  auto &toInsts = to->instructions();

  auto inst = toInsts.empty() ? nullptr : toInsts.back();
  if (inst) {
    inst->dropAllReferences();
    to->eraseInstruction(inst);
  }

  toInsts.insert(toInsts.end(), fromInsts.begin(), fromInsts.end());
  fromInsts.clear();

  for (auto &moved : toInsts) {
    moved->setParent(to.get());
  }
  for (std::size_t i = 0; i < toInsts.size(); ++i) {
    auto &cur = toInsts[i];
    if (!cur) {
      continue;
    }
    cur->setPrev(i == 0 ? nullptr : toInsts[i - 1].get());
    cur->setNext((i + 1) < toInsts.size() ? toInsts[i + 1].get() : nullptr);
  }

  replacePhiIncoming(func, from, to);
  func->eraseBlock(from);
}

inline bool
SimplifyCFG::tryFoldTrivialPhi(ir::BasicBlock &bb,
                               const std::shared_ptr<ir::PhiInst> &phi) {
  if (!phi) {
    return false;
  }

  const auto &incs = phi->incomings();
  if (incs.size() == 1 && incs.front().first) {
    utils::replaceAllUsesWith(phi.get(), incs.front().first.get());
    bb.eraseInstruction(phi);
    return true;
  }

  if (incs.empty()) {
    auto undef = std::make_shared<ir::UndefValue>(phi->type());
    utils::replaceAllUsesWith(phi.get(), undef.get());
    bb.eraseInstruction(phi);
    return true;
  }

  return false;
}

inline bool SimplifyCFG::foldTrivialPhisInBlock(ir::BasicBlock &bb) {
  bool changed = false;
  auto &insts = bb.instructions();
  for (std::size_t i = 0; i < insts.size();) {
    auto phi = std::dynamic_pointer_cast<ir::PhiInst>(insts[i]);
    if (!phi) {
      ++i;
      continue;
    }

    if (tryFoldTrivialPhi(bb, phi)) {
      changed = true;
      continue;
    }
    ++i;
  }
  return changed;
}

inline void SimplifyCFG::rebuildPredecessors(ir::Function &function) {
  for (auto &bb : function.blocks()) {
    bb->clearPredecessors();
  }

  for (auto &bb : function.blocks()) {
    for (auto *succ : utils::detail::successors(*bb)) {
      if (succ) {
        succ->addPredecessor(bb.get());
      }
    }
  }
}

} // namespace rc::opt