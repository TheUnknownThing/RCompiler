#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "type.hpp"

namespace rc::ir {

class AllocaInst : public Instruction {
public:
  AllocaInst(TypePtr allocTy, std::shared_ptr<Value> arraySize = nullptr,
             unsigned alignment = 0, std::string name = {})
      : Instruction(std::make_shared<PointerType>(allocTy), std::move(name)),
        allocTy_(std::move(allocTy)), arraySize_(std::move(arraySize)),
        alignment_(alignment) {
    if (!allocTy_)
      throw std::invalid_argument("AllocaInst requires a valid allocated type");
  }

  const TypePtr &allocatedType() const { return allocTy_; }
  const std::shared_ptr<Value> &arraySize() const { return arraySize_; }
  unsigned alignment() const { return alignment_; }

private:
  TypePtr allocTy_;
  std::shared_ptr<Value> arraySize_;
  unsigned alignment_;
};

class LoadInst : public Instruction {
public:
  LoadInst(std::shared_ptr<Value> ptr, TypePtr resultTy, unsigned alignment = 0,
           bool isVolatile = false, std::string name = {})
      : Instruction(std::move(resultTy), std::move(name)), ptr_(std::move(ptr)),
        alignment_(alignment), isVolatile_(isVolatile) {
    if (!ptr_)
      throw std::invalid_argument("LoadInst requires a pointer operand");
    auto pty = std::dynamic_pointer_cast<const PointerType>(ptr_->type());
    if (!pty)
      throw std::invalid_argument("LoadInst operand must be a pointer");
  }

  const std::shared_ptr<Value> &pointer() const { return ptr_; }
  unsigned alignment() const { return alignment_; }
  bool isVolatile() const { return isVolatile_; }

private:
  std::shared_ptr<Value> ptr_;
  unsigned alignment_;
  bool isVolatile_;
};

class StoreInst : public Instruction {
public:
  StoreInst(std::shared_ptr<Value> val, std::shared_ptr<Value> ptr,
            unsigned alignment = 0, bool isVolatile = false)
      : Instruction(std::make_shared<VoidType>()), val_(std::move(val)),
        ptr_(std::move(ptr)), alignment_(alignment), isVolatile_(isVolatile) {
    if (!ptr_)
      throw std::invalid_argument("StoreInst requires a pointer operand");
    auto pty = std::dynamic_pointer_cast<const PointerType>(ptr_->type());
    if (!pty)
      throw std::invalid_argument("StoreInst pointer must be a pointer type");
  }

  const std::shared_ptr<Value> &value() const { return val_; }
  const std::shared_ptr<Value> &pointer() const { return ptr_; }
  unsigned alignment() const { return alignment_; }
  bool isVolatile() const { return isVolatile_; }

private:
  std::shared_ptr<Value> val_;
  std::shared_ptr<Value> ptr_;
  unsigned alignment_;
  bool isVolatile_;
};

class GetElementPtrInst : public Instruction {
public:
  GetElementPtrInst(TypePtr sourceElemTy, std::shared_ptr<Value> basePtr,
                    std::vector<std::shared_ptr<Value>> indices,
                    bool inbounds = false, std::string name = {})
      : Instruction(std::make_shared<PointerType>(std::move(sourceElemTy)),
                    std::move(name)),
        basePtr_(std::move(basePtr)), indices_(std::move(indices)),
        inbounds_(inbounds) {
    if (!basePtr_)
      throw std::invalid_argument("GetElementPtrInst requires a base pointer");
    if (!std::dynamic_pointer_cast<const PointerType>(basePtr_->type()))
      throw std::invalid_argument("GetElementPtrInst base must be a pointer");
  }

  const std::shared_ptr<Value> &basePointer() const { return basePtr_; }
  const std::vector<std::shared_ptr<Value>> &indices() const {
    return indices_;
  }
  bool inbounds() const { return inbounds_; }

private:
  std::shared_ptr<Value> basePtr_;
  std::vector<std::shared_ptr<Value>> indices_;
  bool inbounds_;
};

} // namespace rc::ir
