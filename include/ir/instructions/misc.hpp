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
  ICmpInst(BasicBlock *parent, ICmpPred pred, std::shared_ptr<Value> lhs,
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

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }

    if (lhs_.get() == oldOp) {
      lhs_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
    if (rhs_.get() == oldOp) {
      rhs_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  cloneInst(BasicBlock *newParent, const ValueRemapMap &valueMap,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<ICmpInst>(newParent, pred_,
                                      remapValue(lhs_, valueMap),
                                      remapValue(rhs_, valueMap), name());
  }

private:
  ICmpPred pred_;
  std::shared_ptr<Value> lhs_;
  std::shared_ptr<Value> rhs_;
};

class CallInst : public Instruction {
public:
  CallInst(BasicBlock *parent, std::shared_ptr<Value> callee,
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

  std::shared_ptr<Function> calleeFunction() const {
    if (!callee_) {
      return nullptr;
    }

    if (auto fn = std::dynamic_pointer_cast<Function>(callee_)) {
      return fn;
    }

    if (auto fnty =
            std::dynamic_pointer_cast<const FunctionType>(callee_->type())) {
      return fnty->function();
    }

    return nullptr;
  }
  const std::shared_ptr<Value> &callee() const { return callee_; }
  const std::vector<std::shared_ptr<Value>> &args() const { return args_; }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }

    if (callee_.get() == oldOp) {
      callee_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
    for (auto &arg : args_) {
      if (arg.get() == oldOp) {
        arg = std::static_pointer_cast<Value>(newOp->shared_from_this());
      }
    }
  }

  std::shared_ptr<Instruction>
  cloneInst(BasicBlock *newParent, const ValueRemapMap &valueMap,
            const BlockRemapMap & /*blockMap*/) const override {
    std::vector<std::shared_ptr<Value>> newArgs;
    newArgs.reserve(args_.size());
    for (const auto &a : args_) {
      newArgs.push_back(remapValue(a, valueMap));
    }
    return std::make_shared<CallInst>(newParent, remapValue(callee_, valueMap),
                                      std::move(newArgs), type(), name());
  }

private:
  std::shared_ptr<Value> callee_;
  std::vector<std::shared_ptr<Value>> args_;
};

class PhiInst : public Instruction {
public:
  using Incoming =
      std::pair<std::shared_ptr<Value>, std::shared_ptr<BasicBlock>>;

  PhiInst(BasicBlock *parent, TypePtr ty,
          std::vector<Incoming> incomings = std::vector<Incoming>{},
          std::string name = {})
      : Instruction(parent, std::move(ty), std::move(name)),
        incomings_(std::move(incomings)) {
    // Register uses for all incoming values
    for (auto &inc : incomings_) {
      if (inc.first) {
        inc.first->addUse(this);
      }
    }
  }

  const std::vector<Incoming> &incomings() const { return incomings_; }
  void addIncoming(std::shared_ptr<Value> v, std::shared_ptr<BasicBlock> bb) {
    if (v) {
      v->addUse(this);
    }
    incomings_.emplace_back(std::move(v), std::move(bb));
  }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }

    for (auto &inc : incomings_) {
      if (inc.first.get() == oldOp) {
        oldOp->removeUse(this);
        newOp->addUse(this);
        inc.first = std::static_pointer_cast<Value>(newOp->shared_from_this());
      }
    }
  }

  void replaceIncomingBlock(std::shared_ptr<BasicBlock> oldBB,
                            std::shared_ptr<BasicBlock> newBB) {
    for (auto &inc : incomings_) {
      if (inc.second == oldBB) {
        inc.second = std::move(newBB);
      }
    }
  }

  void removeIncomingBlock(const BasicBlock *bb) {
    if (!bb) {
      return;
    }
    for (auto it = incomings_.begin(); it != incomings_.end();) {
      if (it->second.get() == bb) {
        if (it->first) {
          it->first->removeUse(this);
        }
        it = incomings_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void dropAllReferences() override {
    for (auto *op : operands) {
      if (op) {
        op->removeUse(this);
      }
    }
    operands.clear();

    for (auto &inc : incomings_) {
      if (inc.first) {
        inc.first->removeUse(this);
      }
    }
    incomings_.clear();
  }

  std::shared_ptr<Instruction>
  cloneInst(BasicBlock *newParent, const ValueRemapMap &valueMap,
            const BlockRemapMap &blockMap) const override {
    std::vector<Incoming> incomings;
    incomings.reserve(incomings_.size());
    for (const auto &inc : incomings_) {
      incomings.emplace_back(remapValue(inc.first, valueMap),
                             remapBlock(inc.second, blockMap));
    }
    return std::make_shared<PhiInst>(newParent, type(), std::move(incomings),
                                     name());
  }

private:
  std::vector<Incoming> incomings_;
};

class SelectInst : public Instruction {
public:
  SelectInst(BasicBlock *parent, std::shared_ptr<Value> cond,
             std::shared_ptr<Value> ifTrue, std::shared_ptr<Value> ifFalse,
             TypePtr ty, std::string name = {})
      : Instruction(parent, std::move(ty), std::move(name)),
        cond_(std::move(cond)), ifTrue_(std::move(ifTrue)),
        ifFalse_(std::move(ifFalse)) {
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

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }

    if (cond_.get() == oldOp) {
      cond_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
    if (ifTrue_.get() == oldOp) {
      ifTrue_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
    if (ifFalse_.get() == oldOp) {
      ifFalse_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  cloneInst(BasicBlock *newParent, const ValueRemapMap &valueMap,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<SelectInst>(
        newParent, remapValue(cond_, valueMap), remapValue(ifTrue_, valueMap),
        remapValue(ifFalse_, valueMap), type(), name());
  }

private:
  std::shared_ptr<Value> cond_;
  std::shared_ptr<Value> ifTrue_;
  std::shared_ptr<Value> ifFalse_;
};

class ZExtInst : public Instruction {
public:
  ZExtInst(BasicBlock *parent, std::shared_ptr<Value> src, TypePtr destTy,
           std::string name = {})
      : Instruction(parent, std::move(destTy), std::move(name)),
        src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("ZExtInst source cannot be null");
    }

    addOperand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }
    if (src_.get() == oldOp) {
      src_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
  }

  TypePtr destType() const { return type(); }

  std::shared_ptr<Instruction>
  cloneInst(BasicBlock *newParent, const ValueRemapMap &valueMap,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<ZExtInst>(newParent, remapValue(src_, valueMap),
                                      destType(), name());
  }

private:
  std::shared_ptr<Value> src_;
};

class SExtInst : public Instruction {
public:
  SExtInst(BasicBlock *parent, std::shared_ptr<Value> src, TypePtr destTy,
           std::string name = {})
      : Instruction(parent, std::move(destTy), std::move(name)),
        src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("SExtInst source cannot be null");
    }

    addOperand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }
    if (src_.get() == oldOp) {
      src_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
  }

  TypePtr destType() const { return type(); }

  std::shared_ptr<Instruction>
  cloneInst(BasicBlock *newParent, const ValueRemapMap &valueMap,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<SExtInst>(newParent, remapValue(src_, valueMap),
                                      destType(), name());
  }

private:
  std::shared_ptr<Value> src_;
};

class TruncInst : public Instruction {
public:
  TruncInst(BasicBlock *parent, std::shared_ptr<Value> src, TypePtr destTy,
            std::string name = {})
      : Instruction(parent, std::move(destTy), std::move(name)),
        src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("TruncInst source cannot be null");
    }

    addOperand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

  TypePtr destType() const { return type(); }

  size_t destBits() const {
    auto destInt = std::dynamic_pointer_cast<const IntegerType>(type());
    if (!destInt) {
      throw std::invalid_argument("TruncInst destination type is not integer");
    }
    return destInt->bits();
  }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }
    if (src_.get() == oldOp) {
      src_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  cloneInst(BasicBlock *newParent, const ValueRemapMap &valueMap,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<TruncInst>(newParent, remapValue(src_, valueMap),
                                       destType(), name());
  }

private:
  std::shared_ptr<Value> src_;
};

} // namespace rc::ir
