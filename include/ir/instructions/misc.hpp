#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "type.hpp"

namespace rc::ir {

enum class ICmpPred {
  EQ,
  NE,
  UGT,
  UGE,
  ULT,
  ULE,
  SGT,
  SGE,
  SLT,
  SLE,
};

class ICmpInst : public Instruction {
public:
  ICmpInst(BasicBlock* parent, ICmpPred pred, std::shared_ptr<Value> lhs,
           std::shared_ptr<Value> rhs, std::string name = {})
      : Instruction(parent, IntegerType::i1(), std::move(name)), pred_(pred),
        lhs_(std::move(lhs)), rhs_(std::move(rhs)) {
    if (!lhs_ || !rhs_) {
      throw std::invalid_argument("ICmpInst operands cannot be null");
    }
    auto li = std::dynamic_pointer_cast<const IntegerType>(lhs_->type());
    auto ri = std::dynamic_pointer_cast<const IntegerType>(rhs_->type());
    if (!li || !ri) {
      throw std::invalid_argument("ICmpInst requires integer operands");
    }
    if (li->bits() != ri->bits()) {
      throw std::invalid_argument(
          "ICmpInst operands must have matching bit width");
    }

    addOperand(lhs_);
    addOperand(rhs_);
  }

  ICmpPred pred() const { return pred_; }
  const std::shared_ptr<Value> &lhs() const { return lhs_; }
  const std::shared_ptr<Value> &rhs() const { return rhs_; }

private:
  ICmpPred pred_;
  std::shared_ptr<Value> lhs_;
  std::shared_ptr<Value> rhs_;
};

class CallInst : public Instruction {
public:
  CallInst(BasicBlock* parent, std::shared_ptr<Value> callee,
           std::vector<std::shared_ptr<Value>> args, TypePtr retTy,
           std::string name = {})
      : Instruction(parent, std::move(retTy), std::move(name)),
        callee_(std::move(callee)), args_(std::move(args)) {

    if (!callee_) {
      throw std::invalid_argument("CallInst requires a callee");
    }
    addOperand(callee_);
    addOperands(args_);
  }

  const std::shared_ptr<Value> &callee() const { return callee_; }
  const std::vector<std::shared_ptr<Value>> &args() const { return args_; }

private:
  std::shared_ptr<Value> callee_;
  std::vector<std::shared_ptr<Value>> args_;
};

class PhiInst : public Instruction {
public:
  using Incoming =
      std::pair<std::shared_ptr<Value>, std::shared_ptr<BasicBlock>>;

  PhiInst(BasicBlock* parent, TypePtr ty, std::vector<Incoming> incomings, std::string name = {})
      : Instruction(parent, std::move(ty), std::move(name)),
        incomings_(std::move(incomings)) {}

  const std::vector<Incoming> &incomings() const { return incomings_; }
  void addIncoming(std::shared_ptr<Value> v, std::shared_ptr<BasicBlock> bb) {
    incomings_.emplace_back(std::move(v), std::move(bb));
  }

private:
  std::vector<Incoming> incomings_;
};

class SelectInst : public Instruction {
public:
  SelectInst(BasicBlock* parent, std::shared_ptr<Value> cond, std::shared_ptr<Value> ifTrue,
             std::shared_ptr<Value> ifFalse, TypePtr ty, std::string name = {})
      : Instruction(parent, std::move(ty), std::move(name)), cond_(std::move(cond)),
        ifTrue_(std::move(ifTrue)), ifFalse_(std::move(ifFalse)) {
    if (!cond_) {
      throw std::invalid_argument("SelectInst requires a condition");
    }
    auto i1 = std::dynamic_pointer_cast<const IntegerType>(cond_->type());
    if (!i1 || i1->bits() != 1) {
      throw std::invalid_argument("SelectInst condition must be i1");
    }

    addOperand(cond_);
    addOperand(ifTrue_);
    addOperand(ifFalse_);
  }

  const std::shared_ptr<Value> &cond() const { return cond_; }
  const std::shared_ptr<Value> &ifTrue() const { return ifTrue_; }
  const std::shared_ptr<Value> &ifFalse() const { return ifFalse_; }

private:
  std::shared_ptr<Value> cond_;
  std::shared_ptr<Value> ifTrue_;
  std::shared_ptr<Value> ifFalse_;
};

class ZExtInst : public Instruction {
public:
  ZExtInst(BasicBlock* parent, std::shared_ptr<Value> src, TypePtr destTy, std::string name = {})
      : Instruction(parent, std::move(destTy), std::move(name)), src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("ZExtInst source cannot be null");
    }

    addOperand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

private:
  std::shared_ptr<Value> src_;
};

class SExtInst : public Instruction {
public:
  SExtInst(BasicBlock* parent, std::shared_ptr<Value> src, TypePtr destTy, std::string name = {})
      : Instruction(parent, std::move(destTy), std::move(name)), src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("SExtInst source cannot be null");
    }

    addOperand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

private:
  std::shared_ptr<Value> src_;
};

class TruncInst : public Instruction {
public:
  TruncInst(BasicBlock* parent, std::shared_ptr<Value> src, TypePtr destTy, std::string name = {})
      : Instruction(parent, std::move(destTy), std::move(name)), src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("TruncInst source cannot be null");
    }

    addOperand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

private:
  std::shared_ptr<Value> src_;
};

} // namespace rc::ir
