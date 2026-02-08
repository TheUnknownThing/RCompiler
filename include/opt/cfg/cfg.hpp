#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "opt/base/baseVisitor.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"

#include "utils/logger.hpp"

namespace rc::opt {

class CFGVisitor : public IRBaseVisitor {
public:
  virtual ~CFGVisitor() = default;

  void run(ir::Module &module) { visit(module); }

  void visit(ir::Value &value) override;

  void visit(ir::Module &module) override;
  void visit(ir::Function &function) override;
  void visit(ir::BasicBlock &basicBlock) override;
  void visit(ir::BranchInst &branchInst) override;
  void visit(ir::ReturnInst &returnInst) override;
};

inline void CFGVisitor::visit(ir::Value &value) {
  if (auto module = dynamic_cast<ir::Module *>(&value)) {
    visit(*module);
  } else if (auto function = dynamic_cast<ir::Function *>(&value)) {
    visit(*function);
  } else if (auto basicBlock = dynamic_cast<ir::BasicBlock *>(&value)) {
    visit(*basicBlock);
  } else if (auto branch = dynamic_cast<ir::BranchInst *>(&value)) {
    visit(*branch);
  } else if (auto ret = dynamic_cast<ir::ReturnInst *>(&value)) {
    visit(*ret);
  }
}

inline void CFGVisitor::visit(ir::Module &module) {
  for (const auto &function : module.functions()) {
    visit(*function);
  }
}

inline void CFGVisitor::visit(ir::Function &function) {
  for (const auto &basicBlock : function.blocks()) {
    basicBlock->clearPredecessors();
  }

  for (const auto &basicBlock : function.blocks()) {
    visit(*basicBlock);
  }

  //   LOG_DEBUG("\n" + rc::opt::utils::cfgToString(function));
  //   LOG_DEBUG("\n" + rc::opt::utils::cfgToDot(function));
}

inline void CFGVisitor::visit(ir::BasicBlock &basicBlock) {
  for (const auto &instruction : basicBlock.instructions()) {
    if (!instruction ||
        dynamic_cast<ir::UnreachableInst *>(instruction.get())) {
      break;
    }
    visit(*instruction);
  }
}

inline void CFGVisitor::visit(ir::BranchInst &branchInst) {
  auto parent = branchInst.parent();
  if (branchInst.isConditional()) {
    branchInst.dest()->addPredecessor(parent);
    branchInst.altDest()->addPredecessor(parent);
  } else {
    branchInst.dest()->addPredecessor(parent);
  }
}

inline void CFGVisitor::visit(ir::ReturnInst &) {
  // exit block, no-op
}

} // namespace rc::opt