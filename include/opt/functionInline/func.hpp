#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "opt/base/baseVisitor.hpp"

namespace rc::opt {

class FunctionInline : public IRBaseVisitor {
public:
  virtual ~FunctionInline() = default;

  void run(ir::Module &module);
  void visit(ir::Value &value) override;
  void visit(ir::Function &function) override;

private:
};

} // namespace rc::opt