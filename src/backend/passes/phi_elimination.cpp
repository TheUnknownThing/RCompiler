#include "backend/passes/phi_elimination.hpp"

namespace rc::backend {

void PhiElimination::run(ir::Module *module) {
  for (auto &function : module->functions()) {
    eliminate_critical_edge(function.get());
  }

  cfg.run(*module);

  for (auto &function : module->functions()) {
    eliminate_phi_in_function(function.get());
  }
}
void PhiElimination::eliminate_critical_edge(ir::Function *func) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &bb : func->blocks()) {
      const auto &succs = opt::utils::detail::successors(*bb);
      if (succs.size() <= 1) {
        continue;
      }
      for (const auto &succ : succs) {
        if (!succ) {
          continue;
        }
        if (succ->predecessors().size() > 1) {
          replace_critical_edge(func, bb.get(), succ);
          changed = true;
        }
      }
      if (changed) {
        break;
      }
    }
  }
}
void PhiElimination::eliminate_phi_in_function(ir::Function *func) {
  for (const auto &bb : func->blocks()) {
    bool changed = true;
    while (changed) {
      changed = false;
      for (const auto &inst : bb->instructions()) {
        auto *phi = dynamic_cast<ir::PhiInst *>(inst.get());
        if (!phi) {
          continue;
        }

        for (const auto &[incoming_val, incoming_bb] : phi->incomings()) {
          incoming_bb->insert_before<ir::MoveInst>(
              incoming_bb->instructions().back(), incoming_val,
              phi->shared_from_this());
        }

        bb->erase_instruction(inst);
        changed = true;
        break;
      }
    }
  }

  for (const auto &bb : func->blocks()) {
    auto &insts = bb->instructions();
    if (insts.size() < 2) {
      continue;
    }

    auto terminator = insts.back();
    if (!terminator || !terminator->is_terminator()) {
      continue;
    }

    std::vector<std::shared_ptr<ir::Instruction>> move_insts;
    for (std::size_t i = insts.size() - 1; i-- > 0;) {
      if (dynamic_cast<ir::MoveInst *>(insts[i].get())) {
        move_insts.push_back(insts[i]);
        continue;
      }
      break;
    }

    if (move_insts.size() <= 1) {
      continue;
    }
    std::reverse(move_insts.begin(), move_insts.end());

    struct Move {
      std::shared_ptr<ir::Value> src;
      std::shared_ptr<ir::Value> dst;
    };

    std::vector<Move> pending;
    for (const auto &sp : move_insts) {
      auto *m = dynamic_cast<ir::MoveInst *>(sp.get());
      if (!m) {
        continue;
      }
      pending.push_back(Move{m->source(), m->destination()});
    }

    for (const auto &sp : move_insts) {
      bb->erase_instruction(sp);
    }

    auto build_source_set = [&](const std::vector<Move> &moves) {
      std::unordered_set<ir::Value *> sources;
      sources.reserve(moves.size());
      for (const auto &m : moves) {
        if (m.src) {
          sources.insert(m.src.get());
        }
      }
      return sources;
    };

    std::vector<Move> scheduled;
    std::size_t tmp_counter = 0;

    while (!pending.empty()) {
      bool progressed = false;
      auto sources = build_source_set(pending);

      for (std::size_t i = 0; i < pending.size(); ++i) {
        auto &m = pending[i];
        if (!m.src || !m.dst) {
          pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
          progressed = true;
          break;
        }

        if (m.src.get() == m.dst.get()) {
          pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
          progressed = true;
          break;
        }

        if (sources.find(m.dst.get()) == sources.end()) {
          scheduled.push_back(m);
          pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
          progressed = true;
          break;
        }
      }

      if (progressed) {
        continue;
      }

      auto &cycle_move = pending.front();
      auto tmp = std::make_shared<ir::Value>(cycle_move.src->type(),
                                             cycle_move.src->name() + "_phi_tmp" +
                                                 std::to_string(tmp_counter++));
      scheduled.push_back(Move{cycle_move.src, tmp});
      cycle_move.src = tmp;
    }

    for (const auto &m : scheduled) {
      if (!m.src || !m.dst || m.src.get() == m.dst.get()) {
        continue;
      }
      bb->insert_before<ir::MoveInst>(terminator, m.src, m.dst);
    }
  }
}
void PhiElimination::replace_critical_edge(ir::Function *func,
                                                ir::BasicBlock *from,
                                                ir::BasicBlock *to) {
  auto new_bb = func->create_block(from->name() + "_to_" + to->name() + "_edge");
  to->replace_predecessor(from, new_bb.get());
  new_bb->add_predecessor(from);

  for (auto &inst : from->instructions()) {
    if (auto branch = std::dynamic_pointer_cast<ir::BranchInst>(inst)) {
      branch->replace_block(to, new_bb.get());
      break;
    } else if (std::dynamic_pointer_cast<ir::ReturnInst>(inst) ||
               std::dynamic_pointer_cast<ir::UnreachableInst>(inst)) {
      break;
    }
  }

  new_bb->append<ir::BranchInst>(to);
}

} // namespace rc::backend
