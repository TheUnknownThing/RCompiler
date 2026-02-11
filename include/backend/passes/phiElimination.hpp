#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"
#include "opt/cfg/cfg.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"

#include <unordered_set>

namespace rc::backend {

class PhiElimination {
public:
  PhiElimination(opt::CFGVisitor &cfg) : cfg(cfg) {}
  void run(ir::Module *module);

private:
  opt::CFGVisitor &cfg;

  void eliminatePhiInFunction(ir::Function *func);
  void eliminateCriticalEdge(ir::Function *func);
  void replaceCriticalEdge(ir::Function *func, ir::BasicBlock *from,
                           ir::BasicBlock *to);
};

inline void PhiElimination::run(ir::Module *module) {
  for (auto &function : module->functions()) {
    eliminateCriticalEdge(function.get());
  }

  cfg.run(*module);

  for (auto &function : module->functions()) {
    eliminatePhiInFunction(function.get());
  }
}

inline void PhiElimination::eliminateCriticalEdge(ir::Function *func) {
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
          replaceCriticalEdge(func, bb.get(), succ);
          changed = true;
        }
      }
      if (changed) {
        break;
      }
    }
  }
}

inline void PhiElimination::eliminatePhiInFunction(ir::Function *func) {
  for (const auto &bb : func->blocks()) {
    bool changed = true;
    while (changed) {
      changed = false;
      for (const auto &inst : bb->instructions()) {
        auto *phi = dynamic_cast<ir::PhiInst *>(inst.get());
        if (!phi) {
          continue;
        }

        for (const auto &[incomingVal, incomingBB] : phi->incomings()) {
          incomingBB->insertBefore<ir::MoveInst>(
              incomingBB->instructions().back(), incomingVal,
              phi->shared_from_this());
        }

        bb->eraseInstruction(inst);
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
    if (!terminator || !terminator->isTerminator()) {
      continue;
    }

    std::vector<std::shared_ptr<ir::Instruction>> moveInsts;
    for (std::size_t i = insts.size() - 1; i-- > 0;) {
      if (dynamic_cast<ir::MoveInst *>(insts[i].get())) {
        moveInsts.push_back(insts[i]);
        continue;
      }
      break;
    }

    if (moveInsts.size() <= 1) {
      continue;
    }
    std::reverse(moveInsts.begin(), moveInsts.end());

    struct Move {
      std::shared_ptr<ir::Value> src;
      std::shared_ptr<ir::Value> dst;
    };

    std::vector<Move> pending;
    for (const auto &sp : moveInsts) {
      auto *m = dynamic_cast<ir::MoveInst *>(sp.get());
      if (!m) {
        continue;
      }
      pending.push_back(Move{m->source(), m->destination()});
    }

    for (const auto &sp : moveInsts) {
      bb->eraseInstruction(sp);
    }

    auto buildSourceSet = [&](const std::vector<Move> &moves) {
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
    std::size_t tmpCounter = 0;

    while (!pending.empty()) {
      bool progressed = false;
      auto sources = buildSourceSet(pending);

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

      auto &cycleMove = pending.front();
      auto tmp = std::make_shared<ir::Value>(cycleMove.src->type(),
                                             cycleMove.src->name() + "_phi_tmp" +
                                                 std::to_string(tmpCounter++));
      scheduled.push_back(Move{cycleMove.src, tmp});
      cycleMove.src = tmp;
    }

    for (const auto &m : scheduled) {
      if (!m.src || !m.dst || m.src.get() == m.dst.get()) {
        continue;
      }
      bb->insertBefore<ir::MoveInst>(terminator, m.src, m.dst);
    }
  }
}

inline void PhiElimination::replaceCriticalEdge(ir::Function *func,
                                                ir::BasicBlock *from,
                                                ir::BasicBlock *to) {
  auto newBB = func->createBlock(from->name() + "_to_" + to->name() + "_edge");
  to->replacePredecessor(from, newBB.get());
  newBB->addPredecessor(from);

  for (auto &inst : from->instructions()) {
    if (auto branch = std::dynamic_pointer_cast<ir::BranchInst>(inst)) {
      branch->replaceBlock(to, newBB.get());
      break;
    } else if (std::dynamic_pointer_cast<ir::ReturnInst>(inst) ||
               std::dynamic_pointer_cast<ir::UnreachableInst>(inst)) {
      break;
    }
  }

  newBB->append<ir::BranchInst>(to);
}

} // namespace rc::backend