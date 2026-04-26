#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "type.hpp"
#include "visitor.hpp"

namespace rc::ir {

enum class BinaryOpKind {
  ADD,
  SUB,
  MUL,
  SDIV,
  UDIV,
  SREM,
  UREM,
  SHL,
  ASHR,
  LSHR,
  AND,
  OR,
  XOR,
};

class BinaryOpInst : public Instruction {
public:
  BinaryOpInst(BasicBlock* parent, BinaryOpKind op, std::shared_ptr<Value> lhs,
               std::shared_ptr<Value> rhs, TypePtr result_type,
               std::string name = {})
      : Instruction(parent, std::move(result_type), std::move(name)), op_(op),
        lhs_(std::move(lhs)), rhs_(std::move(rhs)) {

    if (!lhs_ || !rhs_) {
      throw std::invalid_argument("BinaryOpInst operands cannot be null");
    }
    auto li = std::dynamic_pointer_cast<const IntegerType>(lhs_->type());
    auto ri = std::dynamic_pointer_cast<const IntegerType>(rhs_->type());
    auto ri_ty = std::dynamic_pointer_cast<const IntegerType>(this->type());
    if (!li || !ri || !ri_ty) {
      throw std::invalid_argument("BinaryOpInst requires integer types");
    }

    add_operand(lhs_);
    add_operand(rhs_);
  }

  BinaryOpKind op() const { return op_; }
  const std::shared_ptr<Value> &lhs() const { return lhs_; }
  const std::shared_ptr<Value> &rhs() const { return rhs_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }

    if (lhs_.get() == old_op) {
      lhs_ = std::dynamic_pointer_cast<Value>(new_op->shared_from_this());
    }
    if (rhs_.get() == old_op) {
      rhs_ = std::dynamic_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<BinaryOpInst>(
        new_parent, op_, remap_value(lhs_, value_map), remap_value(rhs_, value_map),
        type(), name());
  }

private:
  BinaryOpKind op_;
  std::shared_ptr<Value> lhs_;
  std::shared_ptr<Value> rhs_;
};

} // namespace rc::ir
