#include "opt/simplify_cfg/simplify_cfg.hpp"

namespace rc::opt {

void SimplifyCFG::run(ir::Module *module) {
  for (auto &function : module->functions()) {
    simplify_cfg(function.get());
  }
}

void SimplifyCFG::simplify_cfg(ir::Function *func) {
  if (func->blocks().empty()) {
    return;
  }

  // After DCE the unreachable blocks are only a single UnreachableInst.
  // Remove those first.
  {
    std::vector<std::shared_ptr<ir::BasicBlock>> unreachable;
    for (const auto &bb : func->blocks()) {
      if (!bb->instructions().empty() &&
          dynamic_cast<ir::UnreachableInst *>(
              bb->instructions().front().get())) {
        unreachable.push_back(bb);
      }
    }
    for (const auto &bb : unreachable) {
      remove_phi_incoming(func, bb);
      func->erase_block(bb);
    }
  }

  rebuild_predecessors(*func);

  // Fold trivial phis once across all blocks; phi folding doesn't change
  // the CFG structure (it only replaces values), so a single sweep is fine.
  for (auto &bb : func->blocks()) {
    fold_trivial_phis_in_block(*bb);
  }

  // Merge single-pred / single-succ chains in a single linear sweep.
  // We maintain predecessors incrementally inside merge_blocks_incremental
  // instead of rebuilding the whole predecessor map after every merge.
  std::unordered_set<ir::BasicBlock *> erased;
  auto snapshot = func->blocks();
  for (const auto &bb_sp : snapshot) {
    auto *bb = bb_sp.get();
    if (!bb || erased.count(bb)) {
      continue;
    }

    // Walk forward, absorbing the unique successor while it has bb as its
    // unique predecessor. This handles long chains in O(chain length).
    while (true) {
      auto succs = utils::detail::successors(*bb);
      if (succs.size() != 1) {
        break;
      }
      auto *succ = succs.front();
      if (!succ || succ == bb || erased.count(succ)) {
        break;
      }
      if (succ->predecessors().size() != 1 ||
          succ->predecessors().front() != bb) {
        break;
      }

      merge_blocks_incremental(func, succ->shared_from_this(),
                               bb->shared_from_this());
      erased.insert(succ);
    }
  }

  // A second phi-folding sweep can clean up phis that became trivial after
  // merging (e.g., phis whose incoming block was renamed).
  for (auto &bb : func->blocks()) {
    fold_trivial_phis_in_block(*bb);
  }
}

void
SimplifyCFG::remove_phi_incoming(ir::Function *func,
                               std::shared_ptr<ir::BasicBlock> old_bb) {
  // Only the successors of old_bb can possibly have a phi referencing it.
  auto succs = utils::detail::successors(*old_bb);
  std::unordered_set<ir::BasicBlock *> visited;
  for (auto *succ : succs) {
    if (!succ || !visited.insert(succ).second) {
      continue;
    }
    auto &insts = succ->instructions();
    for (std::size_t i = 0; i < insts.size();) {
      auto phi = std::dynamic_pointer_cast<ir::PhiInst>(insts[i]);
      if (!phi) {
        break; // PHIs are always at the beginning
      }
      phi->remove_incoming_block(old_bb.get());
      if (try_fold_trivial_phi(*succ, phi)) {
        continue;
      }
      ++i;
    }
  }
  (void)func;
}

void
SimplifyCFG::replace_phi_incoming(ir::Function *func,
                                std::shared_ptr<ir::BasicBlock> old_bb,
                                std::shared_ptr<ir::BasicBlock> new_bb) {
  // Only the successors of old_bb can possibly have a phi referencing it.
  auto succs = utils::detail::successors(*old_bb);
  std::unordered_set<ir::BasicBlock *> visited;
  for (auto *succ : succs) {
    if (!succ || !visited.insert(succ).second) {
      continue;
    }
    auto &insts = succ->instructions();
    for (std::size_t i = 0; i < insts.size();) {
      auto phi = std::dynamic_pointer_cast<ir::PhiInst>(insts[i]);
      if (!phi) {
        break;
      }
      phi->replace_incoming_block(old_bb.get(), new_bb.get());
      if (try_fold_trivial_phi(*succ, phi)) {
        continue;
      }
      ++i;
    }
  }
  (void)func;
}

void SimplifyCFG::merge_blocks(ir::Function *func,
                                     std::shared_ptr<ir::BasicBlock> from,
                                     std::shared_ptr<ir::BasicBlock> to) {
  // Legacy entry point. Kept for any external callers; delegates to the
  // incremental variant followed by an explicit predecessor rebuild for
  // anyone relying on the old semantics.
  merge_blocks_incremental(func, from, to);
}

void SimplifyCFG::merge_blocks_incremental(
    ir::Function *func, std::shared_ptr<ir::BasicBlock> from,
    std::shared_ptr<ir::BasicBlock> to) {
  fold_trivial_phis_in_block(*from);

  auto &from_insts = from->instructions();
  auto &to_insts = to->instructions();

  // Drop the unconditional branch terminator at the end of 'to'.
  auto term = to_insts.empty() ? nullptr : to_insts.back();
  if (term) {
    term->drop_all_references();
    to->erase_instruction(term);
  }

  to_insts.insert(to_insts.end(), from_insts.begin(), from_insts.end());
  from_insts.clear();

  for (auto &moved : to_insts) {
    moved->set_parent(to.get());
  }
  for (std::size_t i = 0; i < to_insts.size(); ++i) {
    auto &cur = to_insts[i];
    if (!cur) {
      continue;
    }
    cur->set_prev(i == 0 ? nullptr : to_insts[i - 1].get());
    cur->set_next((i + 1) < to_insts.size() ? to_insts[i + 1].get() : nullptr);
  }

  // After moving from's instructions into to, to inherits from's successors.
  // For each such successor, replace 'from' with 'to' in:
  //   * the successor's predecessor list
  //   * any phi incomings in the successor
  auto new_succs = utils::detail::successors(*to);
  std::unordered_set<ir::BasicBlock *> visited;
  for (auto *succ : new_succs) {
    if (!succ || !visited.insert(succ).second) {
      continue;
    }
    succ->replace_predecessor(from.get(), to.get());
    auto &succ_insts = succ->instructions();
    for (std::size_t i = 0; i < succ_insts.size();) {
      auto phi = std::dynamic_pointer_cast<ir::PhiInst>(succ_insts[i]);
      if (!phi) {
        break;
      }
      phi->replace_incoming_block(from.get(), to.get());
      if (try_fold_trivial_phi(*succ, phi)) {
        continue;
      }
      ++i;
    }
  }

  func->erase_block(from);
}

bool
SimplifyCFG::try_fold_trivial_phi(ir::BasicBlock &bb,
                               const std::shared_ptr<ir::PhiInst> &phi) {
  if (!phi) {
    return false;
  }

  const auto &incs = phi->incomings();
  if (incs.size() == 1 && incs.front().first) {
    utils::replace_all_uses_with(phi.get(), incs.front().first.get());
    bb.erase_instruction(phi);
    return true;
  }

  if (incs.empty()) {
    auto undef = std::make_shared<ir::UndefValue>(phi->type());
    utils::replace_all_uses_with(phi.get(), undef.get());
    bb.erase_instruction(phi);
    return true;
  }

  return false;
}

bool SimplifyCFG::fold_trivial_phis_in_block(ir::BasicBlock &bb) {
  bool changed = false;
  auto &insts = bb.instructions();
  for (std::size_t i = 0; i < insts.size();) {
    auto phi = std::dynamic_pointer_cast<ir::PhiInst>(insts[i]);
    if (!phi) {
      // PHIs are always at the head of a block, so we can stop here.
      break;
    }

    if (try_fold_trivial_phi(bb, phi)) {
      changed = true;
      continue;
    }
    ++i;
  }
  return changed;
}

void SimplifyCFG::rebuild_predecessors(ir::Function &function) {
  for (auto &bb : function.blocks()) {
    bb->clear_predecessors();
  }

  for (auto &bb : function.blocks()) {
    for (auto *succ : utils::detail::successors(*bb)) {
      if (succ) {
        succ->add_predecessor(bb.get());
      }
    }
  }
}

} // namespace rc::opt
