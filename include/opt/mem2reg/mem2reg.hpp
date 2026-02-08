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

inline void Mem2RegVisitor::run(ir::Module &module) {
  for (const auto &function : module.functions()) {
    dominators_.clear();
    idom_.clear();
    dominanceFrontiers_.clear();
    renameStacks_.clear();
    phiNodes_.clear();
    toRemove_.clear();
    undefValues_.clear();
    promotableAllocas_.clear();

    removeUnusedAllocas(*function);
    replaceUseWithValue(*function);

    findDominators(*function);
    findIDom(*function);
    findDomFrontiers(*function);
    mem2reg(*function);
    removeDeadInstructions(*function);
  }
}

inline void Mem2RegVisitor::removeUnusedAllocas(ir::Function &function) {
  for (const auto &bb : function.blocks()) {
    for (const auto &inst : bb->instructions()) {
      if (auto *alloca = dynamic_cast<ir::AllocaInst *>(inst.get())) {
        if (alloca->getUses().empty()) {
          toRemove_.insert(alloca);
        }
      }
    }
  }
}

inline void Mem2RegVisitor::replaceUseWithValue(ir::Function &function) {
  // if an alloca is only used by 1 store and > 1 load, we can replace loads with the stored value
  std::unordered_map<ir::AllocaInst *, ir::Value *> allocaStoreValues;
  std::unordered_map<ir::AllocaInst *, std::vector<ir::LoadInst *>> allocaLoadInsts;
  std::unordered_map<ir::AllocaInst *, size_t> allocaStoreCounts;

  for (const auto &bb : function.blocks()) {
    for (const auto &inst : bb->instructions()) {
      if (auto *store = dynamic_cast<ir::StoreInst *>(inst.get())) {
        if (auto *alloca = dynamic_cast<ir::AllocaInst *>(store->pointer().get())) {
          allocaStoreCounts[alloca]++;
          allocaStoreValues[alloca] = store->value().get();
        }
      } else if (auto *load = dynamic_cast<ir::LoadInst *>(inst.get())) {
        if (auto *alloca = dynamic_cast<ir::AllocaInst *>(load->pointer().get())) {
          allocaLoadInsts[alloca].push_back(load);
        }
      }
    }
  }

  for (const auto &[alloca, storeCount] : allocaStoreCounts) {
    if (storeCount == 1 && allocaLoadInsts.count(alloca) > 0) {
      bool canReplace = true;
      for (auto *user : alloca->getUses()) {
        if (dynamic_cast<ir::StoreInst *>(user)) {
          continue;
        }
        if (dynamic_cast<ir::LoadInst *>(user)) {
          continue;
        }
        // getelementptr or other use - cannot replace
        canReplace = false;
        break;
      }
      if (!canReplace) {
        continue;
      }

      ir::Value *storedValue = allocaStoreValues[alloca];
      for (auto *loadInst : allocaLoadInsts[alloca]) {
        utils::replaceAllUsesWith(*loadInst, storedValue);
        toRemove_.insert(loadInst);
      }
      // Also remove the store instruction
      for (const auto &bb : function.blocks()) {
        for (const auto &inst : bb->instructions()) {
          if (auto *store = dynamic_cast<ir::StoreInst *>(inst.get())) {
            if (store->pointer().get() == alloca) {
              toRemove_.insert(store);
            }
          }
        }
      }
      toRemove_.insert(alloca);
    }
  }
}

inline void Mem2RegVisitor::findDominators(ir::Function &function) {
  const auto &blocks = function.blocks();
  if (blocks.empty()) {
    return;
  }

  auto all_blocks = std::unordered_set<ir::BasicBlock *>{};
  for (const auto &bb : blocks) {
    all_blocks.insert(bb.get());
  }

  for (const auto &bb : blocks) {
    auto &doms = dominators_[bb.get()];
    if (bb.get() == blocks.front().get()) {
      doms.insert(bb.get());
    } else {
      doms = all_blocks;
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &bb : blocks) {
      if (bb.get() == blocks.front().get()) {
        continue;
      }
      auto new_doms = all_blocks;
      // intersect dominators of predecessors
      for (const auto &pred_bb : bb->predecessors()) {
        const auto &pred_doms = dominators_[pred_bb];
        auto intersection = std::unordered_set<ir::BasicBlock *>{};
        for (const auto &d : new_doms) {
          if (pred_doms.count(d)) {
            intersection.insert(d);
          }
        }
        new_doms = std::move(intersection);
      }
      new_doms.insert(bb.get());
      if (new_doms != dominators_[bb.get()]) {
        dominators_[bb.get()] = std::move(new_doms);
        changed = true;
      }
    }
  }
}

inline void Mem2RegVisitor::findIDom(ir::Function &function) {
  const auto &blocks = function.blocks();
  if (blocks.empty()) {
    return;
  }

  for (const auto &bb : blocks) {
    if (bb.get() == blocks.front().get()) {
      idom_[bb.get()] = nullptr; // entry block has no idom
      continue;
    }

    auto &bb_doms = dominators_[bb.get()];
    ir::BasicBlock *idom = nullptr;
    for (const auto &d : bb_doms) {
      if (d == bb.get()) {
        continue;
      }
      if (idom == nullptr) {
        idom = d;
      } else if (dominators_[d].size() == dominators_[bb.get()].size() - 1) {
        idom = d;
      }
    }

    if (!idom) {
      throw std::runtime_error("Failed to find immediate dominator");
    }

    idom_[bb.get()] = idom;
  }
}

inline void Mem2RegVisitor::findDomFrontiers(ir::Function &function) {
  const auto &blocks = function.blocks();
  if (blocks.empty()) {
    return;
  }

  for (const auto &bb : blocks) {
    auto preds = bb->predecessors();
    if (preds.size() < 2) {
      continue;
    }

    for (const auto &p : preds) {
      for (const auto &elem : dominators_[p]) {
        if (dominators_[bb.get()].count(elem) == 0) {
          dominanceFrontiers_[elem].insert(bb.get());
        }
      }
    }
  }
}

inline void Mem2RegVisitor::mem2reg(ir::Function &function) {
  const auto &blocks = function.blocks();
  if (blocks.empty()) {
    return;
  }

  std::unordered_map<ir::AllocaInst *, std::unordered_set<ir::BasicBlock *>>
      defBlocks;
  std::vector<ir::AllocaInst *> allocas;

  for (const auto &bb : blocks) {
    for (const auto &inst : bb->instructions()) {
      if (auto *alloca = dynamic_cast<ir::AllocaInst *>(inst.get())) {
        allocas.push_back(alloca);
        continue;
      }

      if (auto *store = dynamic_cast<ir::StoreInst *>(inst.get())) {
        if (auto *alloca =
                dynamic_cast<ir::AllocaInst *>(store->pointer().get())) {
          defBlocks[alloca].insert(bb.get());
        }
      }
    }
  }

  // Compute which allocas are safe to promote.
  // We may pass a ptr to another function, so we need to be careful.
  for (auto *alloca : allocas) {
    bool promotable = true;
    for (auto *user : alloca->getUses()) {
      if (auto *load = dynamic_cast<ir::LoadInst *>(user)) {
        if (load->pointer().get() == alloca) {
          continue;
        }
      }
      if (auto *store = dynamic_cast<ir::StoreInst *>(user)) {
        if (store->pointer().get() == alloca) {
          continue;
        }
      }

      promotable = false;
      break;
    }

    if (promotable) {
      promotableAllocas_.insert(alloca);
    }
  }

  for (auto *alloca : allocas) {
    if (!promotableAllocas_.count(alloca)) {
      continue;
    }
    const auto &defs = defBlocks[alloca];
    for (auto *defBB : defs) {
      placePhiNodes(*defBB, alloca);
    }
  }

  rename(*blocks.front());
}

inline void Mem2RegVisitor::placePhiNodes(ir::BasicBlock &bb,
                                          ir::AllocaInst *alloca) {

  // place it at bb's dominance frontiers
  const auto &frontiers = dominanceFrontiers_[&bb];
  for (auto *df_bb : frontiers) {
    if (phiNodes_[df_bb].count(alloca) || df_bb == &bb) {
      continue; // already placed
    }

    auto phi = df_bb->prepend<ir::PhiInst>(alloca->allocatedType());
    phiNodes_[df_bb][alloca] = phi.get();

    placePhiNodes(*df_bb, alloca);
  }
}

inline void Mem2RegVisitor::rename(ir::BasicBlock &bb) {
  std::vector<const ir::AllocaInst *> pushedAllocas;

  const auto &phi_bb = phiNodes_[&bb];
  for (const auto &[alloca, phi] : phi_bb) {
    renameStacks_[alloca].push_back(phi);
    pushedAllocas.push_back(alloca);
  }

  for (const auto &inst : bb.instructions()) {
    if (auto *store = dynamic_cast<ir::StoreInst *>(inst.get())) {
      if (auto *alloca =
              dynamic_cast<ir::AllocaInst *>(store->pointer().get())) {
        if (!promotableAllocas_.count(alloca)) {
          continue;
        }
        auto &stack = renameStacks_[alloca];
        stack.push_back(store->value().get());
        pushedAllocas.push_back(alloca);

        LOG_DEBUG("[Mem2Reg] Replacing store to alloca " + alloca->name() +
                  " with value " + store->value()->name());

        toRemove_.insert(inst.get());
      }
    } else if (auto *load = dynamic_cast<ir::LoadInst *>(inst.get())) {
      if (auto *alloca =
              dynamic_cast<ir::AllocaInst *>(load->pointer().get())) {
        if (!promotableAllocas_.count(alloca)) {
          continue;
        }
        auto &stack = renameStacks_[alloca];
        if (stack.empty()) {
          // No reaching definition - use undef value
          auto undef =
              std::make_shared<ir::UndefValue>(alloca->allocatedType());
          undefValues_.push_back(undef); // Keep alive
          utils::replaceAllUsesWith(*load, undef.get());
        } else {
          utils::replaceAllUsesWith(*load, stack.back());
        }

        toRemove_.insert(inst.get());
      }
    }
  }

  // rename in successor phi nodes
  const auto &succs = utils::detail::successors(bb);
  for (auto *succ_bb : succs) {
    const auto &phi_succ_bb = phiNodes_[succ_bb];
    for (const auto &[alloca, phi] : phi_succ_bb) {
      auto &stack = renameStacks_[alloca];
      if (stack.empty()) {
        // No reaching definition - use undef value
        auto undef = std::make_shared<ir::UndefValue>(alloca->allocatedType());
        undefValues_.push_back(undef); // Keep alive
        phi->addIncoming(undef, bb.shared_from_this());
      } else {
        phi->addIncoming(stack.back()->shared_from_this(),
                         bb.shared_from_this());
      }
    }
  }

  // visit children in dominator tree
  for (auto &child_bb_pair : idom_) {
    if (child_bb_pair.second == &bb) {
      rename(*child_bb_pair.first);
    }
  }

  // pop
  for (auto it = pushedAllocas.rbegin(); it != pushedAllocas.rend(); ++it) {
    auto &stack = renameStacks_[*it];
    if (!stack.empty()) {
      stack.pop_back();
    }
  }
}

inline void Mem2RegVisitor::removeDeadInstructions(ir::Function &function) {
  for (const auto &bb : function.blocks()) {
    auto &instrs = bb->instructions();
    // NOTE: upon removing the instructions, we also need to adjust its next &
    // prev ptr.
    for (auto it = instrs.begin(); it != instrs.end();) {
      if (toRemove_.count(it->get())) {
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
      } else {
        ++it;
      }
    }
  }
}

} // namespace rc::opt