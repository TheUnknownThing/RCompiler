#include "opt/cfg/cfg.hpp"

namespace rc::opt {

void CFGVisitor::visit(ir::Value &value) {
  if (auto module = dynamic_cast<ir::Module *>(&value)) {
    visit(*module);
  } else if (auto function = dynamic_cast<ir::Function *>(&value)) {
    visit(*function);
  } else if (auto basic_block = dynamic_cast<ir::BasicBlock *>(&value)) {
    visit(*basic_block);
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
  for (const auto &basic_block : function.blocks()) {
    basic_block->clear_predecessors();
  }

  for (const auto &basic_block : function.blocks()) {
    visit(*basic_block);
  }

  //   LOG_DEBUG("\n" + rc::opt::utils::cfgToString(function));
  //   LOG_DEBUG("\n" + rc::opt::utils::cfgToDot(function));
}
void CFGVisitor::visit(ir::BasicBlock &basic_block) {
  for (const auto &instruction : basic_block.instructions()) {
    if (!instruction ||
        dynamic_cast<ir::UnreachableInst *>(instruction.get())) {
      break;
    }
    visit(*instruction);
  }
}
void CFGVisitor::visit(ir::BranchInst &branch_inst) {
  auto parent = branch_inst.parent();
  if (branch_inst.is_conditional()) {
    if (branch_inst.dest()) {
      branch_inst.dest()->add_predecessor(parent);
    }
    if (branch_inst.alt_dest()) {
      branch_inst.alt_dest()->add_predecessor(parent);
    }
  } else {
    if (branch_inst.dest()) {
      branch_inst.dest()->add_predecessor(parent);
    }
  }
}
void CFGVisitor::visit(ir::ReturnInst &) {
  // exit block, no-op
}

} // namespace rc::opt
