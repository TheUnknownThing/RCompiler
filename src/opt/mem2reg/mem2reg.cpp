#include "opt/mem2reg/mem2reg.hpp"

namespace rc::opt {

void Mem2RegVisitor::run(ir::Module &module) {
  for (const auto &function : module.functions()) {
    dominators_.clear();
    idom_.clear();
    dominance_frontiers_.clear();
    rename_stacks_.clear();
    phi_nodes_.clear();
    to_remove_.clear();
    undef_values_.clear();
    promotable_allocas_.clear();

    remove_unused_allocas(*function);
    replace_use_with_value(*function);

    find_dominators(*function);
    find_i_dom(*function);
    find_dom_frontiers(*function);
    mem2reg(*function);
    remove_dead_instructions(*function);
  }
}

void Mem2RegVisitor::remove_unused_allocas(ir::Function &function) {
  for (const auto &bb : function.blocks()) {
    for (const auto &inst : bb->instructions()) {
      if (auto *alloca = dynamic_cast<ir::AllocaInst *>(inst.get())) {
        if (alloca->get_uses().empty()) {
          to_remove_.insert(alloca);
        }
      }
    }
  }
}

void Mem2RegVisitor::replace_use_with_value(ir::Function &function) {
  // if an alloca is only used by 1 store and > 1 load, we can replace loads with the stored value
  std::unordered_map<ir::AllocaInst *, ir::Value *> alloca_store_values;
  std::unordered_map<ir::AllocaInst *, std::vector<ir::LoadInst *>> alloca_load_insts;
  std::unordered_map<ir::AllocaInst *, size_t> alloca_store_counts;

  for (const auto &bb : function.blocks()) {
    for (const auto &inst : bb->instructions()) {
      if (auto *store = dynamic_cast<ir::StoreInst *>(inst.get())) {
        if (auto *alloca = dynamic_cast<ir::AllocaInst *>(store->pointer().get())) {
          alloca_store_counts[alloca]++;
          alloca_store_values[alloca] = store->value().get();
        }
      } else if (auto *load = dynamic_cast<ir::LoadInst *>(inst.get())) {
        if (auto *alloca = dynamic_cast<ir::AllocaInst *>(load->pointer().get())) {
          alloca_load_insts[alloca].push_back(load);
        }
      }
    }
  }

  for (const auto &[alloca, store_count] : alloca_store_counts) {
    if (store_count == 1 && alloca_load_insts.count(alloca) > 0) {
      bool can_replace = true;
      for (auto *user : alloca->get_uses()) {
        if (auto *store = dynamic_cast<ir::StoreInst *>(user)) {
          if (store->pointer().get() != alloca) {
            can_replace = false;
            break;
          }
          continue;
        }
        if (auto *load = dynamic_cast<ir::LoadInst *>(user)) {
          if (load->pointer().get() != alloca) {
            can_replace = false;
            break;
          }
          continue;
        }
        // getelementptr or other use - cannot replace
        can_replace = false;
        break;
      }
      if (!can_replace) {
        continue;
      }

      ir::Value *stored_value = alloca_store_values[alloca];
      for (auto *load_inst : alloca_load_insts[alloca]) {
        utils::replace_all_uses_with(*load_inst, stored_value);
        to_remove_.insert(load_inst);
      }
      // Also remove the store instruction
      for (const auto &bb : function.blocks()) {
        for (const auto &inst : bb->instructions()) {
          if (auto *store = dynamic_cast<ir::StoreInst *>(inst.get())) {
            if (store->pointer().get() == alloca) {
              to_remove_.insert(store);
            }
          }
        }
      }
      to_remove_.insert(alloca);
    }
  }
}

void Mem2RegVisitor::find_dominators(ir::Function &function) {
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

void Mem2RegVisitor::find_i_dom(ir::Function &function) {
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

void Mem2RegVisitor::find_dom_frontiers(ir::Function &function) {
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
          dominance_frontiers_[elem].insert(bb.get());
        }
      }
    }
  }
}

void Mem2RegVisitor::mem2reg(ir::Function &function) {
  const auto &blocks = function.blocks();
  if (blocks.empty()) {
    return;
  }

  std::unordered_map<ir::AllocaInst *, std::unordered_set<ir::BasicBlock *>>
      def_blocks;
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
          def_blocks[alloca].insert(bb.get());
        }
      }
    }
  }

  // Compute which allocas are safe to promote.
  // We may pass a ptr to another function, so we need to be careful.
  for (auto *alloca : allocas) {
    bool promotable = true;
    for (auto *user : alloca->get_uses()) {
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
      promotable_allocas_.insert(alloca);
    }
  }

  for (auto *alloca : allocas) {
    if (!promotable_allocas_.count(alloca)) {
      continue;
    }
    const auto &defs = def_blocks[alloca];
    for (auto *def_bb : defs) {
      place_phi_nodes(*def_bb, alloca);
    }
  }

  rename(*blocks.front());
}

void Mem2RegVisitor::place_phi_nodes(ir::BasicBlock &bb,
                                          ir::AllocaInst *alloca) {

  // place it at bb's dominance frontiers
  const auto &frontiers = dominance_frontiers_[&bb];
  for (auto *df_bb : frontiers) {
    if (phi_nodes_[df_bb].count(alloca) || df_bb == &bb) {
      continue; // already placed
    }

    auto phi = df_bb->prepend<ir::PhiInst>(alloca->allocated_type());
    phi_nodes_[df_bb][alloca] = phi.get();

    place_phi_nodes(*df_bb, alloca);
  }
}

void Mem2RegVisitor::rename(ir::BasicBlock &bb) {
  std::vector<const ir::AllocaInst *> pushed_allocas;

  const auto &phi_bb = phi_nodes_[&bb];
  for (const auto &[alloca, phi] : phi_bb) {
    rename_stacks_[alloca].push_back(phi);
    pushed_allocas.push_back(alloca);
  }

  for (const auto &inst : bb.instructions()) {
    if (auto *store = dynamic_cast<ir::StoreInst *>(inst.get())) {
      if (auto *alloca =
              dynamic_cast<ir::AllocaInst *>(store->pointer().get())) {
        if (!promotable_allocas_.count(alloca)) {
          continue;
        }
        auto &stack = rename_stacks_[alloca];
        stack.push_back(store->value().get());
        pushed_allocas.push_back(alloca);

        LOG_DEBUG("[Mem2Reg] Replacing store to alloca " + alloca->name() +
                  " with value " + store->value()->name());

        to_remove_.insert(inst.get());
      }
    } else if (auto *load = dynamic_cast<ir::LoadInst *>(inst.get())) {
      if (auto *alloca =
              dynamic_cast<ir::AllocaInst *>(load->pointer().get())) {
        if (!promotable_allocas_.count(alloca)) {
          continue;
        }
        auto &stack = rename_stacks_[alloca];
        if (stack.empty()) {
          // No reaching definition - use undef value
          auto undef =
              std::make_shared<ir::UndefValue>(alloca->allocated_type());
          undef_values_.push_back(undef); // Keep alive
          utils::replace_all_uses_with(*load, undef.get());
        } else {
          utils::replace_all_uses_with(*load, stack.back());
        }

        to_remove_.insert(inst.get());
      }
    }
  }

  // rename in successor phi nodes
  const auto &succs = utils::detail::successors(bb);
  for (auto *succ_bb : succs) {
    const auto &phi_succ_bb = phi_nodes_[succ_bb];
    for (const auto &[alloca, phi] : phi_succ_bb) {
      auto &stack = rename_stacks_[alloca];
      if (stack.empty()) {
        // No reaching definition - use undef value
        auto undef = std::make_shared<ir::UndefValue>(alloca->allocated_type());
        undef_values_.push_back(undef); // Keep alive
        phi->add_incoming(undef, &bb);
      } else {
        phi->add_incoming(stack.back()->shared_from_this(),
                         &bb);
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
  for (auto it = pushed_allocas.rbegin(); it != pushed_allocas.rend(); ++it) {
    auto &stack = rename_stacks_[*it];
    if (!stack.empty()) {
      stack.pop_back();
    }
  }
}

void Mem2RegVisitor::remove_dead_instructions(ir::Function &function) {
  for (const auto &bb : function.blocks()) {
    auto &instrs = bb->instructions();
    // NOTE: upon removing the instructions, we also need to adjust its next &
    // prev ptr.
    for (auto it = instrs.begin(); it != instrs.end();) {
      if (to_remove_.count(it->get())) {
        auto inst = std::static_pointer_cast<ir::Instruction>(*it);
        inst->drop_all_references();

        auto *prev = inst->prev();
        auto *next = inst->next();
        if (prev) {
          prev->set_next(next);
        }
        if (next) {
          next->set_prev(prev);
        }
        it = instrs.erase(it);
      } else {
        ++it;
      }
    }
  }
}

} // namespace rc::opt
