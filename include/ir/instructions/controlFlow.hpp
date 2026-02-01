#pragma once

#include <memory>
#include <stdexcept>

#include "type.hpp"

namespace rc::ir {

class BranchInst : public Instruction {
public:
  explicit BranchInst(BasicBlock* parent, std::shared_ptr<BasicBlock> dest)
      : Instruction(parent, std::make_shared<VoidType>()), isCond_(false),
        dest_(std::move(dest)) {}

  BranchInst(BasicBlock* parent, std::shared_ptr<Value> cond, std::shared_ptr<BasicBlock> ifTrue,
             std::shared_ptr<BasicBlock> ifFalse)
      : Instruction(parent, std::make_shared<VoidType>()), isCond_(true),
        cond_(std::move(cond)), dest_(std::move(ifTrue)),
        altDest_(std::move(ifFalse)) {
    if (!cond_) {
      throw std::invalid_argument(
          "BranchInst conditional requires a condition");
    }
    auto i1 = std::dynamic_pointer_cast<const IntegerType>(cond_->type());
    if (!i1 || i1->bits() != 1) {
      throw std::invalid_argument("BranchInst condition must be i1");
    }

    addOperand(cond_);
  }

  bool isConditional() const { return isCond_; }
  const std::shared_ptr<Value> &cond() const { return cond_; }
  std::shared_ptr<BasicBlock> &dest() { return dest_; }
  const std::shared_ptr<BasicBlock> &dest() const { return dest_; }
  std::shared_ptr<BasicBlock> &altDest() { return altDest_; }
  const std::shared_ptr<BasicBlock> &altDest() const { return altDest_; }

private:
  bool isCond_;
  std::shared_ptr<Value> cond_;
  std::shared_ptr<BasicBlock> dest_;    // unconditional target
  std::shared_ptr<BasicBlock> altDest_; // false target when conditional
};

class ReturnInst : public Instruction {
public:
  ReturnInst(BasicBlock* parent) : Instruction(parent, std::make_shared<VoidType>()) {}
  explicit ReturnInst(BasicBlock* parent, std::shared_ptr<Value> val)
      : Instruction(parent, std::make_shared<VoidType>()), val_(std::move(val)) {
    addOperand(val_);
  }

  bool isVoid() const { return val_ == nullptr; }
  const std::shared_ptr<Value> &value() const { return val_; }

private:
  std::shared_ptr<Value> val_;
};

class UnreachableInst : public Instruction {
public:
  UnreachableInst(BasicBlock* parent) : Instruction(parent, std::make_shared<VoidType>()) {}
};

} // namespace rc::ir
