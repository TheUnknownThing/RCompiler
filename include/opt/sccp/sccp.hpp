#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"

#include "context.hpp"
#include "opt/base/base_visitor.hpp"
#include "opt/utils/cfg_pretty_print.hpp"
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
  void visit(ir::BasicBlock &basic_block) override;

  void visit(ir::BinaryOpInst &binary_op_inst) override;
  void visit(ir::BranchInst &branch_inst) override;
  void visit(ir::UnreachableInst &unreachable_inst) override;
  void visit(ir::ReturnInst &return_inst) override;
  void visit(ir::AllocaInst &alloca_inst) override;
  void visit(ir::LoadInst &load_inst) override;
  void visit(ir::StoreInst &store_inst) override;
  void visit(ir::GetElementPtrInst &get_element_ptr_inst) override;
  void visit(ir::ICmpInst &icmp_inst) override;
  void visit(ir::SExtInst &sext_inst) override;
  void visit(ir::ZExtInst &zext_inst) override;
  void visit(ir::TruncInst &trunc_inst) override;
  void visit(ir::CallInst &call_inst) override;
  void visit(ir::PhiInst &phi_inst) override;
  void visit(ir::SelectInst &select_inst) override;

private:
  std::vector<ir::Instruction *> instruction_worklist_;
  std::vector<std::pair<const ir::BasicBlock *, const ir::BasicBlock *>> edges_;

  std::unordered_map<ir::Value *, LatticeValueKind> lattice_values_;
  std::unordered_map<ir::Value *, std::shared_ptr<ir::Constant>>
      constant_values_;
  std::unordered_set<const ir::BasicBlock *> executable_blocks_;

  std::unordered_set<ir::Instruction *> instructions_to_remove_;

  ConstantContext *context_{nullptr};

  void remove_dead_instructions(ir::Function &function);
  void remove_dead_blocks(ir::Function &function);

  LatticeValueKind get_lattice_value(ir::Value *value) {
    if (!value) {
      return LatticeValueKind::UNDEF;
    }

    if (auto *c = dynamic_cast<ir::Constant *>(value)) {
      lattice_values_[value] = LatticeValueKind::CONSTANT;
      if (constant_values_.find(value) == constant_values_.end()) {
        constant_values_[value] = std::dynamic_pointer_cast<ir::Constant>(
            c->shared_from_this());
      }
      return LatticeValueKind::CONSTANT;
    }

    auto it = lattice_values_.find(value);
    if (it != lattice_values_.end()) {
      return it->second;
    }
    return LatticeValueKind::UNDEF;
  }

  LatticeValueKind evaluate_kind(ir::Value *value_1, ir::Value *value_2) {
    auto kind1 = get_lattice_value(value_1);
    auto kind2 = get_lattice_value(value_2);

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
  merge_phi_values(LatticeValueKind kind1, ir::Value *value1,
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
      auto const1 = constant_values_[value1];
      auto const2 = constant_values_[value2];
      if (const1->equals(*const2)) {
        return {LatticeValueKind::CONSTANT, const1};
      } else {
        return {LatticeValueKind::OVERDEF, nullptr};
      }
    } else if (kind1 == LatticeValueKind::CONSTANT &&
               kind2 == LatticeValueKind::UNDEF) {
      return {LatticeValueKind::CONSTANT, constant_values_[value1]};
    } else if (kind1 == LatticeValueKind::UNDEF &&
               kind2 == LatticeValueKind::CONSTANT) {
      return {LatticeValueKind::CONSTANT, constant_values_[value2]};
    } else {
      return {LatticeValueKind::OVERDEF, nullptr};
    }
  }
};






















} // namespace rc::opt
