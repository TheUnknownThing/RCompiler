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





} // namespace rc::backend