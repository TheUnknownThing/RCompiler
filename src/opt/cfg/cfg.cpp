#include "opt/cfg/cfg.hpp"

namespace rc::opt {

void CFGVisitor::visit(ir::Value &value) {
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
void CFGVisitor::visit(ir::Module &module) {
  for (const auto &function : module.functions()) {
    visit(*function);
  }
}
void CFGVisitor::visit(ir::Function &function) {
  for (const auto &basicBlock : function.blocks()) {
    basicBlock->clearPredecessors();
  }

  for (const auto &basicBlock : function.blocks()) {
    visit(*basicBlock);
  }

  //   LOG_DEBUG("\n" + rc::opt::utils::cfgToString(function));
  //   LOG_DEBUG("\n" + rc::opt::utils::cfgToDot(function));
}
void CFGVisitor::visit(ir::BasicBlock &basicBlock) {
  for (const auto &instruction : basicBlock.instructions()) {
    if (!instruction ||
        dynamic_cast<ir::UnreachableInst *>(instruction.get())) {
      break;
    }
    visit(*instruction);
  }
}
void CFGVisitor::visit(ir::BranchInst &branchInst) {
  auto parent = branchInst.parent();
  if (branchInst.isConditional()) {
    if (branchInst.dest()) {
      branchInst.dest()->addPredecessor(parent);
    }
    if (branchInst.altDest()) {
      branchInst.altDest()->addPredecessor(parent);
    }
  } else {
    if (branchInst.dest()) {
      branchInst.dest()->addPredecessor(parent);
    }
  }
}
void CFGVisitor::visit(ir::ReturnInst &) {
  // exit block, no-op
}

} // namespace rc::opt
