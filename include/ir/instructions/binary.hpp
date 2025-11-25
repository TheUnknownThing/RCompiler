#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "type.hpp"

namespace rc::ir {

enum class BinaryOpKind {
  ADD,
  SUB,
  MUL,
  SDIV,
  SREM,
  SHL,
  ASHR,
  AND,
  OR,
  XOR,
};

class BinaryOpInst : public Instruction {
public:
  BinaryOpInst(BinaryOpKind op, std::shared_ptr<Value> lhs,
               std::shared_ptr<Value> rhs, TypePtr resultType,
               std::string name = {})
      : Instruction(std::move(resultType), std::move(name)), op_(op),
        lhs_(std::move(lhs)), rhs_(std::move(rhs)) {

    if (!lhs_ || !rhs_) {
      throw std::invalid_argument("BinaryOpInst operands cannot be null");
    }
    auto li = std::dynamic_pointer_cast<const IntegerType>(lhs_->type());
    auto ri = std::dynamic_pointer_cast<const IntegerType>(rhs_->type());
    auto riTy = std::dynamic_pointer_cast<const IntegerType>(this->type());
    if (!li || !ri || !riTy) {
      throw std::invalid_argument("BinaryOpInst requires integer types");
    }
  }

  BinaryOpKind op() const { return op_; }
  const std::shared_ptr<Value> &lhs() const { return lhs_; }
  const std::shared_ptr<Value> &rhs() const { return rhs_; }

private:
  BinaryOpKind op_;
  std::shared_ptr<Value> lhs_;
  std::shared_ptr<Value> rhs_;
};

} // namespace rc::ir
