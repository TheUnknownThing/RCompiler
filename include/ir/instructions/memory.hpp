#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "type.hpp"

namespace rc::ir {

class AllocaInst : public Instruction {
public:
  AllocaInst(BasicBlock *parent, TypePtr allocTy,
             std::shared_ptr<Value> arraySize = nullptr, unsigned alignment = 0,
             std::string name = {})
      : Instruction(parent, std::make_shared<PointerType>(allocTy),
                    std::move(name)),
        allocTy_(std::move(allocTy)), arraySize_(std::move(arraySize)),
        alignment_(alignment) {
    if (!allocTy_)
      throw std::invalid_argument("AllocaInst requires a valid allocated type");

    if (arraySize_)
      addOperand(arraySize_);
  }

  const TypePtr &allocatedType() const { return allocTy_; }
  const std::shared_ptr<Value> &arraySize() const { return arraySize_; }
  unsigned alignment() const { return alignment_; }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }

    if (arraySize_.get() == oldOp) {
      arraySize_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
  }

private:
  TypePtr allocTy_;
  std::shared_ptr<Value> arraySize_;
  unsigned alignment_;
};

class LoadInst : public Instruction {
public:
  LoadInst(BasicBlock *parent, std::shared_ptr<Value> ptr, TypePtr resultTy,
           unsigned alignment = 0, bool isVolatile = false,
           std::string name = {})
      : Instruction(parent, std::move(resultTy), std::move(name)),
        ptr_(std::move(ptr)), alignment_(alignment), isVolatile_(isVolatile) {
    if (!ptr_)
      throw std::invalid_argument("LoadInst requires a pointer operand");
    auto pty = std::dynamic_pointer_cast<const PointerType>(ptr_->type());
    if (!pty)
      throw std::invalid_argument("LoadInst operand must be a pointer");
    addOperand(ptr_);
  }

  const std::shared_ptr<Value> &pointer() const { return ptr_; }
  unsigned alignment() const { return alignment_; }
  bool isVolatile() const { return isVolatile_; }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }

    if (ptr_.get() == oldOp) {
      ptr_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
  }

private:
  std::shared_ptr<Value> ptr_;
  unsigned alignment_;
  bool isVolatile_;
};

class StoreInst : public Instruction {
public:
  StoreInst(BasicBlock *parent, std::shared_ptr<Value> val,
            std::shared_ptr<Value> ptr, unsigned alignment = 0,
            bool isVolatile = false)
      : Instruction(parent, std::make_shared<VoidType>()), val_(std::move(val)),
        ptr_(std::move(ptr)), alignment_(alignment), isVolatile_(isVolatile) {
    if (!ptr_)
      throw std::invalid_argument("StoreInst requires a pointer operand");
    auto pty = std::dynamic_pointer_cast<const PointerType>(ptr_->type());
    if (!pty)
      throw std::invalid_argument("StoreInst pointer must be a pointer type");

    addOperand(val_);
    addOperand(ptr_);
  }

  std::shared_ptr<Value> &value() { return val_; }
  const std::shared_ptr<Value> &value() const { return val_; }
  std::shared_ptr<Value> &pointer() { return ptr_; }
  const std::shared_ptr<Value> &pointer() const { return ptr_; }
  unsigned alignment() const { return alignment_; }
  bool isVolatile() const { return isVolatile_; }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }

    if (val_.get() == oldOp) {
      val_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
    if (ptr_.get() == oldOp) {
      ptr_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
  }

private:
  std::shared_ptr<Value> val_;
  std::shared_ptr<Value> ptr_;
  unsigned alignment_;
  bool isVolatile_;
};

class GetElementPtrInst : public Instruction {
public:
  GetElementPtrInst(BasicBlock *parent, TypePtr sourceElemTy,
                    std::shared_ptr<Value> basePtr,
                    std::vector<std::shared_ptr<Value>> indices,
                    std::string name = {})
      : Instruction(parent,
                    std::make_shared<PointerType>(std::move(sourceElemTy)),
                    std::move(name)),
        basePtr_(std::move(basePtr)), indices_(std::move(indices)) {
    if (!basePtr_)
      throw std::invalid_argument("GetElementPtrInst requires a base pointer");
    if (!std::dynamic_pointer_cast<const PointerType>(basePtr_->type()))
      throw std::invalid_argument("GetElementPtrInst base must be a pointer");

    addOperand(basePtr_);
    addOperands(indices_);
  }

  const std::shared_ptr<Value> &basePointer() const { return basePtr_; }
  const std::vector<std::shared_ptr<Value>> &indices() const {
    return indices_;
  }

  void replaceOperand(Value *oldOp, Value *newOp) override {
    for (auto &op : operands) {
      if (op == oldOp) {
        oldOp->removeUse(this);
        op = newOp;
        newOp->addUse(this);
      }
    }

    if (basePtr_.get() == oldOp) {
      basePtr_ = std::static_pointer_cast<Value>(newOp->shared_from_this());
    }
    for (std::size_t i = 0; i < indices_.size(); ++i) {
      if (indices_[i].get() == oldOp) {
        indices_[i] = std::static_pointer_cast<Value>(newOp->shared_from_this());
      }
    }
  }

private:
  std::shared_ptr<Value> basePtr_;
  std::vector<std::shared_ptr<Value>> indices_;
};

} // namespace rc::ir
