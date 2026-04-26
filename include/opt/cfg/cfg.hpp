#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"
#include "ir/instructions/visitor.hpp"

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







} // namespace rc::opt