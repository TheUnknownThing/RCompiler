#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"
#include "opt/cfg/cfg.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"

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
    for (const auto &inst : bb->instructions()) {
      auto *phi = dynamic_cast<ir::PhiInst *>(inst.get());
      if (!phi) {
        continue;
      }

      for (const auto &[incomingVal, incomingBB] : phi->incomings()) {
        incomingBB->append<ir::MoveInst>(incomingVal, phi->shared_from_this());
      }
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