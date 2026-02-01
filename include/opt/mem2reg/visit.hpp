#pragma once

#include <ir/instructions/controlFlow.hpp>
#include <ir/instructions/memory.hpp>
#include <ir/instructions/misc.hpp>
#include <ir/instructions/topLevel.hpp>
#include <ir/instructions/type.hpp>

#include <opt/base/baseVisitor.hpp>

namespace rc::opt {
class Mem2RegVisitor : public IRBaseVisitor {
public:
  virtual ~Mem2RegVisitor() = default;
  void run(ir::Module &module) { visit(module); }
  void visit(ir::Value &value) override;
  void visit(ir::Module &module) override;
  void visit(ir::Function &function) override;
  void visit(ir::BasicBlock &basicBlock) override;
};
} // namespace rc::opt