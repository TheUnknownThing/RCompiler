#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "opt/base/baseVisitor.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"

#include "utils/logger.hpp"

namespace rc::opt {

enum class LatticeValueKind {
  UNDEF,
  CONSTANT,
  OVERDEF
}; // overdef means variable

class SCCPVisitor : public IRBaseVisitor {
public:
  virtual ~SCCPVisitor() = default;
  void run(ir::Module &module);
  void visit(ir::Function &function) override;

private:
  std::vector<ir::BasicBlock *> controlFlowWorklist_;
  std::vector<ir::Instruction *> instructionWorklist_;
  std::pair<ir::BasicBlock *, ir::BasicBlock *> edges_;

  std::unordered_map<ir::Value *, LatticeValueKind> latticeValues_;
  std::unordered_map<ir::Value *, std::shared_ptr<ir::Constant>>
      constantValues_;
};

inline void SCCPVisitor::run(ir::Module &module) {
  for (const auto &function : module.functions()) {
    visit(*function);
  }
}

inline void SCCPVisitor::visit(ir::Function &function) {
  LOG_DEBUG("Starting SCCP on function: " + function.name());
}

} // namespace rc::opt
