#pragma once

#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"
#include "ir/instructions/visitor.hpp"

#include "opt/base/base_visitor.hpp"
#include "opt/utils/cfg_pretty_print.hpp"

#include "utils/logger.hpp"

namespace rc::opt {

class CFGVisitor : public IRBaseVisitor {
public:
  virtual ~CFGVisitor() = default;

  void run(ir::Module &module) { visit(module); }

  void visit(ir::Value &value) override;

  void visit(ir::Module &module) override;
  void visit(ir::Function &function) override;
  void visit(ir::BasicBlock &basic_block) override;
  void visit(ir::BranchInst &branch_inst) override;
  void visit(ir::ReturnInst &return_inst) override;
};

} // namespace rc::opt