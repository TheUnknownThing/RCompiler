#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "type.hpp"
#include "visitor.hpp"

namespace rc::ir {

class AllocaInst : public Instruction {
public:
  AllocaInst(BasicBlock *parent, TypePtr alloc_ty,
             std::shared_ptr<Value> array_size = nullptr, unsigned alignment = 0,
             std::string name = {})
      : Instruction(parent, std::make_shared<PointerType>(alloc_ty),
                    std::move(name)),
        alloc_ty_(std::move(alloc_ty)), array_size_(std::move(array_size)),
        alignment_(alignment) {
    if (!alloc_ty_)
      throw std::invalid_argument("AllocaInst requires a valid allocated type");

    if (array_size_)
      add_operand(array_size_);
  }

  const TypePtr &allocated_type() const { return alloc_ty_; }
  const std::shared_ptr<Value> &array_size() const { return array_size_; }
  unsigned alignment() const { return alignment_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }

    if (array_size_.get() == old_op) {
      array_size_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<AllocaInst>(new_parent, alloc_ty_,
                                        remap_value(array_size_, value_map),
                                        alignment_, name());
  }

private:
  TypePtr alloc_ty_;
  std::shared_ptr<Value> array_size_;
  unsigned alignment_;
};

class LoadInst : public Instruction {
public:
  LoadInst(BasicBlock *parent, std::shared_ptr<Value> ptr, TypePtr result_ty,
           unsigned alignment = 0, std::string name = {})
      : Instruction(parent, std::move(result_ty), std::move(name)),
        ptr_(std::move(ptr)), alignment_(alignment) {
    if (!ptr_)
      throw std::invalid_argument("LoadInst requires a pointer operand");
    auto pty = std::dynamic_pointer_cast<const PointerType>(ptr_->type());
    if (!pty)
      throw std::invalid_argument("LoadInst operand must be a pointer");
    add_operand(ptr_);
  }

  const std::shared_ptr<Value> &pointer() const { return ptr_; }
  unsigned alignment() const { return alignment_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }

    if (ptr_.get() == old_op) {
      ptr_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<LoadInst>(new_parent, remap_value(ptr_, value_map),
                                      type(), alignment_, name());
  }

private:
  std::shared_ptr<Value> ptr_;
  unsigned alignment_;
};

class StoreInst : public Instruction {
public:
  StoreInst(BasicBlock *parent, std::shared_ptr<Value> val,
            std::shared_ptr<Value> ptr, unsigned alignment = 0,
            bool is_volatile = false)
      : Instruction(parent, std::make_shared<VoidType>()), val_(std::move(val)),
        ptr_(std::move(ptr)), alignment_(alignment), is_volatile_(is_volatile) {
    if (!ptr_)
      throw std::invalid_argument("StoreInst requires a pointer operand");
    auto pty = std::dynamic_pointer_cast<const PointerType>(ptr_->type());
    if (!pty)
      throw std::invalid_argument("StoreInst pointer must be a pointer type");

    add_operand(val_);
    add_operand(ptr_);
  }

  std::shared_ptr<Value> &value() { return val_; }
  const std::shared_ptr<Value> &value() const { return val_; }
  std::shared_ptr<Value> &pointer() { return ptr_; }
  const std::shared_ptr<Value> &pointer() const { return ptr_; }
  unsigned alignment() const { return alignment_; }
  bool is_volatile() const { return is_volatile_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }

    if (val_.get() == old_op) {
      val_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
    if (ptr_.get() == old_op) {
      ptr_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<StoreInst>(new_parent, remap_value(val_, value_map),
                                       remap_value(ptr_, value_map), alignment_,
                                       is_volatile_);
  }

private:
  std::shared_ptr<Value> val_;
  std::shared_ptr<Value> ptr_;
  unsigned alignment_;
  bool is_volatile_;
};

class GetElementPtrInst : public Instruction {
public:
  GetElementPtrInst(BasicBlock *parent, TypePtr source_elem_ty,
                    std::shared_ptr<Value> base_ptr,
                    std::vector<std::shared_ptr<Value>> indices,
                    std::string name = {})
      : Instruction(parent,
                    std::make_shared<PointerType>(std::move(source_elem_ty)),
                    std::move(name)),
        base_ptr_(std::move(base_ptr)), indices_(std::move(indices)) {
    if (!base_ptr_)
      throw std::invalid_argument("GetElementPtrInst requires a base pointer");
    if (!std::dynamic_pointer_cast<const PointerType>(base_ptr_->type()))
      throw std::invalid_argument("GetElementPtrInst base must be a pointer");

    add_operand(base_ptr_);
    add_operands(indices_);
  }

  const std::shared_ptr<Value> &base_pointer() const { return base_ptr_; }
  const std::vector<std::shared_ptr<Value>> &indices() const {
    return indices_;
  }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }

    if (base_ptr_.get() == old_op) {
      base_ptr_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
    for (std::size_t i = 0; i < indices_.size(); ++i) {
      if (indices_[i].get() == old_op) {
        indices_[i] =
            std::static_pointer_cast<Value>(new_op->shared_from_this());
      }
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    auto pty = std::dynamic_pointer_cast<const PointerType>(type());
    if (!pty) {
      throw std::invalid_argument(
          "GetElementPtrInst clone requires pointer result type");
    }

    std::vector<std::shared_ptr<Value>> new_idx;
    new_idx.reserve(indices_.size());
    for (const auto &idx : indices_) {
      new_idx.push_back(remap_value(idx, value_map));
    }

    return std::make_shared<GetElementPtrInst>(new_parent, pty->pointee(),
                                               remap_value(base_ptr_, value_map),
                                               std::move(new_idx), name());
  }

private:
  std::shared_ptr<Value> base_ptr_;
  std::vector<std::shared_ptr<Value>> indices_;
};

} // namespace rc::ir
