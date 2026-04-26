#pragma once

#include <memory>
#include <stdexcept>

#include "type.hpp"
#include "visitor.hpp"

namespace rc::ir {

class BranchInst : public Instruction {
public:
  explicit BranchInst(BasicBlock *parent, BasicBlock *dest)
      : Instruction(parent, std::make_shared<VoidType>()), is_cond_(false),
        dest_(dest) {}

  BranchInst(BasicBlock *parent, std::shared_ptr<Value> cond,
             BasicBlock *if_true, BasicBlock *if_false)
      : Instruction(parent, std::make_shared<VoidType>()), is_cond_(true),
        cond_(std::move(cond)), dest_(if_true), alt_dest_(if_false) {
    if (!cond_) {
      throw std::invalid_argument(
          "BranchInst conditional requires a condition");
    }
    auto i1 = std::dynamic_pointer_cast<const IntegerType>(cond_->type());
    if (!i1 || i1->bits() != 1) {
      throw std::invalid_argument("BranchInst condition must be i1");
    }

    add_operand(cond_);
  }

  bool is_conditional() const { return is_cond_; }
  const std::shared_ptr<Value> &cond() const { return cond_; }
  BasicBlock *dest() const { return dest_; }
  void set_dest(BasicBlock *bb) { dest_ = bb; }
  BasicBlock *alt_dest() const { return alt_dest_; }
  void set_alt_dest(BasicBlock *bb) { alt_dest_ = bb; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }
  bool is_terminator() const override { return true; }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }

    if (cond_.get() == old_op) {
      cond_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  void replace_block(BasicBlock *old_bb, BasicBlock *new_bb) {
    if (dest_ == old_bb) {
      dest_ = new_bb;
    }
    if (alt_dest_ == old_bb) {
      alt_dest_ = new_bb;
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap &block_map) const override {
    if (!is_cond_) {
      return std::make_shared<BranchInst>(new_parent,
                                          remap_block(dest_, block_map));
    }
    return std::make_shared<BranchInst>(new_parent, remap_value(cond_, value_map),
                                        remap_block(dest_, block_map),
                                        remap_block(alt_dest_, block_map));
  }

private:
  bool is_cond_;
  std::shared_ptr<Value> cond_;
  BasicBlock *dest_{nullptr};    // unconditional/true target
  BasicBlock *alt_dest_{nullptr}; // false target when conditional
};

class ReturnInst : public Instruction {
public:
  ReturnInst(BasicBlock *parent)
      : Instruction(parent, std::make_shared<VoidType>()) {}
  explicit ReturnInst(BasicBlock *parent, std::shared_ptr<Value> val)
      : Instruction(parent, std::make_shared<VoidType>()),
        val_(std::move(val)) {
    add_operand(val_);
  }

  bool is_void() const { return val_ == nullptr; }
  const std::shared_ptr<Value> &value() const { return val_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }
  bool is_terminator() const override { return true; }

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
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    if (is_void()) {
      return std::make_shared<ReturnInst>(new_parent);
    }
    return std::make_shared<ReturnInst>(new_parent, remap_value(val_, value_map));
  }

  std::shared_ptr<Value> &value() { return val_; }

private:
  std::shared_ptr<Value> val_;
};

class UnreachableInst : public Instruction {
public:
  UnreachableInst(BasicBlock *parent)
      : Instruction(parent, std::make_shared<VoidType>()) {}

  void accept(InstructionVisitor &v) const override { v.visit(*this); }
  bool is_terminator() const override { return true; }

  void replace_operand(Value * /*oldOp*/, Value * /*newOp*/) override {
    // No operands to replace
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap & /*valueMap*/,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<UnreachableInst>(new_parent);
  }
};

} // namespace rc::ir
