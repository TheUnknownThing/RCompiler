#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "context.hpp"
#include "opt/base/baseVisitor.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"
#include "opt/utils/ir_utils.hpp"

#include "utils/logger.hpp"
#include <memory>

namespace rc::opt {

enum class LatticeValueKind {
  UNDEF,
  CONSTANT,
  OVERDEF
}; // overdef means variable

class SCCPVisitor : public IRBaseVisitor {
public:
  virtual ~SCCPVisitor() = default;
  explicit SCCPVisitor(ConstantContext *context) : context_(context) {}

  void run(ir::Module &module);

  void visit(ir::Value &value) override;

  void visit(ir::Function &function) override;
  void visit(ir::BasicBlock &basicBlock) override;

  void visit(ir::BinaryOpInst &binaryOpInst) override;
  void visit(ir::BranchInst &branchInst) override;
  void visit(ir::UnreachableInst &unreachableInst) override;
  void visit(ir::ReturnInst &returnInst) override;
  void visit(ir::AllocaInst &allocaInst) override;
  void visit(ir::LoadInst &loadInst) override;
  void visit(ir::StoreInst &storeInst) override;
  void visit(ir::GetElementPtrInst &getElementPtrInst) override;
  void visit(ir::ICmpInst &icmpInst) override;
  void visit(ir::SExtInst &sextInst) override;
  void visit(ir::ZExtInst &zextInst) override;
  void visit(ir::TruncInst &truncInst) override;
  void visit(ir::CallInst &callInst) override;
  void visit(ir::PhiInst &phiInst) override;
  void visit(ir::SelectInst &selectInst) override;

private:
  std::vector<ir::Instruction *> instructionWorklist_;
  std::vector<std::pair<const ir::BasicBlock *, const ir::BasicBlock *>> edges_;

  std::unordered_map<ir::Value *, LatticeValueKind> latticeValues_;
  std::unordered_map<ir::Value *, std::shared_ptr<ir::Constant>>
      constantValues_;
  std::unordered_set<const ir::BasicBlock *> executableBlocks_;

  std::unordered_set<ir::Instruction *> instructionsToRemove_;

  ConstantContext *context_{nullptr};

  void removeDeadInstructions(ir::Function &function);
  void removeDeadBlocks(ir::Function &function);

  LatticeValueKind getLatticeValue(ir::Value *value) {
    if (!value) {
      return LatticeValueKind::UNDEF;
    }

    if (auto *c = dynamic_cast<ir::Constant *>(value)) {
      latticeValues_[value] = LatticeValueKind::CONSTANT;
      if (constantValues_.find(value) == constantValues_.end()) {
        constantValues_[value] = std::dynamic_pointer_cast<ir::Constant>(
            c->shared_from_this());
      }
      return LatticeValueKind::CONSTANT;
    }

    auto it = latticeValues_.find(value);
    if (it != latticeValues_.end()) {
      return it->second;
    }
    return LatticeValueKind::UNDEF;
  }

  LatticeValueKind evaluateKind(ir::Value *value_1, ir::Value *value_2) {
    auto kind1 = getLatticeValue(value_1);
    auto kind2 = getLatticeValue(value_2);

    if (kind1 == LatticeValueKind::OVERDEF ||
        kind2 == LatticeValueKind::OVERDEF) {
      return LatticeValueKind::OVERDEF;
    }
    if (kind1 == LatticeValueKind::UNDEF || kind2 == LatticeValueKind::UNDEF) {
      return LatticeValueKind::UNDEF;
    }
    return LatticeValueKind::CONSTANT;
  }

  std::pair<LatticeValueKind, std::shared_ptr<ir::Constant>>
  mergePHIValues(LatticeValueKind kind1, ir::Value *value1,
                 LatticeValueKind kind2, ir::Value *value2) {
    if (kind1 == LatticeValueKind::UNDEF && kind2 == LatticeValueKind::UNDEF) {
      return {LatticeValueKind::UNDEF, nullptr};
    } else if (kind1 == LatticeValueKind::OVERDEF ||
               kind2 == LatticeValueKind::OVERDEF) {
      return {LatticeValueKind::OVERDEF, nullptr};
    } else if (kind1 == LatticeValueKind::CONSTANT &&
               kind2 == LatticeValueKind::CONSTANT) {
      if (!value1 || !value2) {
        return {LatticeValueKind::OVERDEF, nullptr};
      }
      auto const1 = constantValues_[value1];
      auto const2 = constantValues_[value2];
      if (const1->equals(*const2)) {
        return {LatticeValueKind::CONSTANT, const1};
      } else {
        return {LatticeValueKind::OVERDEF, nullptr};
      }
    } else if (kind1 == LatticeValueKind::CONSTANT &&
               kind2 == LatticeValueKind::UNDEF) {
      return {LatticeValueKind::CONSTANT, constantValues_[value1]};
    } else if (kind1 == LatticeValueKind::UNDEF &&
               kind2 == LatticeValueKind::CONSTANT) {
      return {LatticeValueKind::CONSTANT, constantValues_[value2]};
    } else {
      return {LatticeValueKind::OVERDEF, nullptr};
    }
  }
};






















} // namespace rc::opt
