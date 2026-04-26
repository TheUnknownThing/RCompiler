#include "opt/dce/dce.hpp"

namespace rc::opt {

void DeadCodeElimVisitor::run(ir::Module &module) {
  for (const auto &function : module.functions()) {
    for (const auto &bb : function->blocks()) {
      trim_after_terminator(*bb);
    }

    fold_constant_conditional_branches(*function);

    auto reachable = compute_reachable(*function);
    squash_unreachable_blocks(*function, reachable);

    rebuild_predecessors(*function);
    remove_undef_phi_incoming_blocks(*function);
    aggressive_dead_code_elimination(*function);
  }
}

void DeadCodeElimVisitor::trim_after_terminator(ir::BasicBlock &bb) {
  auto &instrs = bb.instructions();
  bool found_terminator = false;

  for (auto it = instrs.begin(); it != instrs.end();) {
    auto *inst = it->get();
    if (!inst) {
      ++it;
      continue;
    }

    if (found_terminator) {
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
      continue;
    }

    if (dynamic_cast<ir::BranchInst *>(inst) ||
        dynamic_cast<ir::ReturnInst *>(inst) ||
        dynamic_cast<ir::UnreachableInst *>(inst)) {
      found_terminator = true;
    }

    ++it;
  }
}

void DeadCodeElimVisitor::fold_constant_conditional_branches(
    ir::Function &function) {
  for (const auto &bb_ptr : function.blocks()) {
    if (!bb_ptr) {
      continue;
    }

    auto &bb = *bb_ptr;
    auto &instrs = bb.instructions();
    if (instrs.empty()) {
      continue;
    }

    for (auto it = instrs.begin(); it != instrs.end(); ++it) {
      auto *br = dynamic_cast<ir::BranchInst *>(it->get());
      if (!br) {
        continue;
      }
      if (!br->is_conditional()) {
        break;
      }

      auto *ci = dynamic_cast<ir::ConstantInt *>(br->cond().get());
      if (!ci) {
        break;
      }

      const bool take_true = (ci->value() != 0);
      auto chosen = take_true ? br->dest() : br->alt_dest();
      auto other = take_true ? br->alt_dest() : br->dest();
      if (!chosen) {
        break;
      }

      // Replace the conditional branch with an unconditional one.
      auto old_inst = std::static_pointer_cast<ir::Instruction>(*it);
      old_inst->drop_all_references();

      auto *prev = old_inst->prev();
      auto *next = old_inst->next();
      if (prev) {
        prev->set_next(next);
      }
      if (next) {
        next->set_prev(prev);
      }

      it = instrs.erase(it);

      auto new_br = std::make_shared<ir::BranchInst>(&bb, chosen);
      new_br->set_prev(prev);
      new_br->set_next(next);
      if (prev) {
        prev->set_next(new_br.get());
      }
      if (next) {
        next->set_prev(new_br.get());
      }

      instrs.insert(it, std::move(new_br));

      if (other) {
        for (const auto &inst2 : other->instructions()) {
          if (!inst2) {
            break;
          }
          auto *phi = dynamic_cast<ir::PhiInst *>(inst2.get());
          if (!phi) {
            break; // PHIs are always at the beginning
          }
          phi->remove_incoming_block(&bb);
        }
      }
      break;
    }
  }
}

std::unordered_set<ir::BasicBlock *>
DeadCodeElimVisitor::compute_reachable(ir::Function &function) {
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

void DeadCodeElimVisitor::squash_unreachable_blocks(
    ir::Function &function,
    const std::unordered_set<ir::BasicBlock *> &reachable) {
  for (const auto &bb : function.blocks()) {
    if (reachable.count(bb.get())) {
      continue;
    }

    // This block will no longer branch anywhere; remove it from successor PHIs.
    auto succs = utils::detail::successors(*bb);
    for (auto *succ_const : succs) {
      auto *succ = const_cast<ir::BasicBlock *>(succ_const);
      for (const auto &inst2 : succ->instructions()) {
        if (!inst2) {
          break;
        }
        auto *phi = dynamic_cast<ir::PhiInst *>(inst2.get());
        if (!phi) {
          break;
        }
        phi->remove_incoming_block(bb.get());
      }
    }

    auto &instrs = bb->instructions();
    for (auto &inst : instrs) {
      if (inst) {
        std::static_pointer_cast<ir::Instruction>(inst)->drop_all_references();
      }
    }
    instrs.clear();
    bb->append<ir::UnreachableInst>();
  }
}

void DeadCodeElimVisitor::rebuild_predecessors(ir::Function &function) {
  // Clear existing predecessor lists.
  for (const auto &bb : function.blocks()) {
    bb->clear_predecessors();
  }

  // Add predecessors based on branch terminators.
  for (const auto &bb : function.blocks()) {
    auto succs = utils::detail::successors(*bb);
    for (auto *succ : succs) {
      auto *succ_nonconst = const_cast<ir::BasicBlock *>(succ);
      succ_nonconst->add_predecessor(bb.get());
    }
  }
}

void DeadCodeElimVisitor::remove_undef_phi_incoming_blocks(
    ir::Function &function) {
  for (const auto &bb : function.blocks()) {
    std::unordered_set<ir::BasicBlock *> predecessors;
    for (auto *pred : bb->predecessors()) {
      predecessors.insert(pred);
    }

    for (const auto &inst : bb->instructions()) {
      if (!inst) {
        break;
      }
      auto *phi = dynamic_cast<ir::PhiInst *>(inst.get());
      if (!phi) {
        break;
      }

      auto &incomings = phi->incomings();
      for (auto it = incomings.begin(); it != incomings.end();) {
        auto *incoming_block = it->second;
        if (!incoming_block || !predecessors.count(incoming_block)) {
          if (it->first) {
            it->first->remove_use(phi);
          }
          it = incomings.erase(it);
        } else {
          ++it;
        }
      }
    }
  }
}

void DeadCodeElimVisitor::aggressive_dead_code_elimination(
    ir::Function &function) {
  std::unordered_set<ir::Instruction *> live;
  std::vector<ir::Instruction *> worklist;

  for (const auto &bb : function.blocks()) {
    if (!bb) {
      continue;
    }
    for (const auto &inst : bb->instructions()) {
      if (inst && is_live_root(*inst)) {
        mark_live(inst.get(), live, worklist);
      }
    }
  }

  while (!worklist.empty()) {
    auto *inst = worklist.back();
    worklist.pop_back();
    if (!inst) {
      continue;
    }

    for (auto *operand : inst->get_operands()) {
      if (auto *operand_inst = dynamic_cast<ir::Instruction *>(operand)) {
        mark_live(operand_inst, live, worklist);
      }
    }

    if (auto *phi = dynamic_cast<ir::PhiInst *>(inst)) {
      for (const auto &incoming : phi->incomings()) {
        if (auto *incoming_inst =
                dynamic_cast<ir::Instruction *>(incoming.first.get())) {
          mark_live(incoming_inst, live, worklist);
        }
      }
    }
  }

  for (const auto &bb : function.blocks()) {
    if (!bb) {
      continue;
    }

    auto &instrs = bb->instructions();
    for (auto it = instrs.begin(); it != instrs.end();) {
      auto *inst = it->get();
      if (!inst || live.count(inst)) {
        ++it;
        continue;
      }

      auto doomed = *it;
      doomed->drop_all_references();

      auto *prev = doomed->prev();
      auto *next = doomed->next();
      if (prev) {
        prev->set_next(next);
      }
      if (next) {
        next->set_prev(prev);
      }

      it = instrs.erase(it);
    }
  }
}

bool DeadCodeElimVisitor::is_live_root(const ir::Instruction &inst) const {
  if (inst.is_terminator()) {
    return true;
  }
  if (dynamic_cast<const ir::StoreInst *>(&inst) ||
      dynamic_cast<const ir::CallInst *>(&inst) ||
      dynamic_cast<const ir::MoveInst *>(&inst)) {
    return true;
  }
  return false;
}

void DeadCodeElimVisitor::mark_live(
    ir::Instruction *inst, std::unordered_set<ir::Instruction *> &live,
    std::vector<ir::Instruction *> &worklist) const {
  if (!inst) {
    return;
  }
  if (live.insert(inst).second) {
    worklist.push_back(inst);
  }
}

} // namespace rc::opt
