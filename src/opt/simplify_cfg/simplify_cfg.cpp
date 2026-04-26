#include "opt/simplify_cfg/simplify_cfg.hpp"

namespace rc::opt {

void SimplifyCFG::run(ir::Module *module) {
  for (auto &function : module->functions()) {
    simplify_cfg(function.get());
  }
}
void SimplifyCFG::simplify_cfg(ir::Function *func) {
  std::unordered_set<ir::BasicBlock *> unreachable;
  if (func->blocks().empty()) {
    return;
  }

  // after DCE, the unrechable blocks are only with one UnreachableInst
  auto prev_blocks = func->blocks();
  for (const auto &bb : prev_blocks) {
    if (auto inst = bb->instructions().empty()
                        ? nullptr
                        : std::dynamic_pointer_cast<ir::UnreachableInst>(
                              bb->instructions().front())) {
      unreachable.insert(bb.get());
    }
  }

  for (const auto &bb : unreachable) {
    remove_phi_incoming(func, bb->shared_from_this());
    func->erase_block(bb->shared_from_this());
  }

  rebuild_predecessors(*func);

  bool changed = true;
  while (changed) {
    changed = false;

    for (auto &bb : func->blocks()) {
      changed |= fold_trivial_phis_in_block(*bb);
    }
    if (changed) {
      continue;
    }

    rebuild_predecessors(*func);
    for (auto &bb : func->blocks()) {
      auto succs = utils::detail::successors(*bb);
      if (succs.size() == 1) {
        auto succ = succs.front();
        if (succ->predecessors().size() == 1 &&
            succ->predecessors().front() == bb.get()) {
          auto succ_bb = succ->shared_from_this();
          merge_blocks(func, succ_bb, bb);
          rebuild_predecessors(*func);
          changed = true;
          break;
        }
      }
    }
  }
}
void
SimplifyCFG::remove_phi_incoming(ir::Function *func,
                               std::shared_ptr<ir::BasicBlock> old_bb) {
  for (const auto &bb : func->blocks()) {
    auto &insts = bb->instructions();
    for (std::size_t i = 0; i < insts.size();) {
      auto phi = std::dynamic_pointer_cast<ir::PhiInst>(insts[i]);
      if (!phi) {
        ++i;
        continue;
      }

      phi->remove_incoming_block(old_bb.get());
      if (try_fold_trivial_phi(*bb, phi)) {
        continue;
      }

      ++i;
    }
  }
}
void
SimplifyCFG::replace_phi_incoming(ir::Function *func,
                                std::shared_ptr<ir::BasicBlock> old_bb,
                                std::shared_ptr<ir::BasicBlock> new_bb) {
  for (const auto &bb : func->blocks()) {
    auto &insts = bb->instructions();
    for (std::size_t i = 0; i < insts.size();) {
      auto phi = std::dynamic_pointer_cast<ir::PhiInst>(insts[i]);
      if (!phi) {
        ++i;
        continue;
      }

      phi->replace_incoming_block(old_bb.get(), new_bb.get());
      if (try_fold_trivial_phi(*bb, phi)) {
        continue;
      }

      ++i;
    }
  }
}
void SimplifyCFG::merge_blocks(ir::Function *func,
                                     std::shared_ptr<ir::BasicBlock> from,
                                     std::shared_ptr<ir::BasicBlock> to) {
  fold_trivial_phis_in_block(*from);

  // Move instructions from 'from' to 'to'
  auto &from_insts = from->instructions();
  auto &to_insts = to->instructions();

  auto inst = to_insts.empty() ? nullptr : to_insts.back();
  if (inst) {
    inst->drop_all_references();
    to->erase_instruction(inst);
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

  replace_phi_incoming(func, from, to);
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
      ++i;
      continue;
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
