#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "top_level.hpp"
#include "type.hpp"
#include "visitor.hpp"

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

    add_operand(lhs_);
    add_operand(rhs_);
  }

  ICmpPred pred() const { return pred_; }
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
      lhs_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
    if (rhs_.get() == old_op) {
      rhs_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<ICmpInst>(new_parent, pred_,
                                      remap_value(lhs_, value_map),
                                      remap_value(rhs_, value_map), name());
  }

private:
  ICmpPred pred_;
  std::shared_ptr<Value> lhs_;
  std::shared_ptr<Value> rhs_;
};

class CallInst : public Instruction {
public:
  CallInst(BasicBlock *parent, std::shared_ptr<Value> callee,
           std::vector<std::shared_ptr<Value>> args, TypePtr ret_ty,
           std::string name = {})
      : Instruction(parent, std::move(ret_ty), std::move(name)),
        callee_(std::move(callee)), args_(std::move(args)) {

    if (!callee_) {
      throw std::invalid_argument("CallInst requires a callee");
    }
    add_operand(callee_);
    add_operands(args_);
  }

  Function *callee_function() const {
    if (!callee_) {
      return nullptr;
    }

    if (auto fn = dynamic_cast<Function *>(callee_.get())) {
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

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }

    if (callee_.get() == old_op) {
      callee_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
    for (auto &arg : args_) {
      if (arg.get() == old_op) {
        arg = std::static_pointer_cast<Value>(new_op->shared_from_this());
      }
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    std::vector<std::shared_ptr<Value>> new_args;
    new_args.reserve(args_.size());
    for (const auto &a : args_) {
      new_args.push_back(remap_value(a, value_map));
    }
    return std::make_shared<CallInst>(new_parent, remap_value(callee_, value_map),
                                      std::move(new_args), type(), name());
  }

private:
  std::shared_ptr<Value> callee_;
  std::vector<std::shared_ptr<Value>> args_;
};

class PhiInst : public Instruction {
public:
  using Incoming = std::pair<std::shared_ptr<Value>, BasicBlock *>;

  PhiInst(BasicBlock *parent, TypePtr ty,
          std::vector<Incoming> incomings = std::vector<Incoming>{},
          std::string name = {})
      : Instruction(parent, std::move(ty), std::move(name)),
        incomings_(std::move(incomings)) {
    // Register uses for all incoming values
    for (auto &inc : incomings_) {
      if (inc.first) {
        inc.first->add_use(this);
      }
    }
  }

  const std::vector<Incoming> &incomings() const { return incomings_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void add_incoming(std::shared_ptr<Value> v, BasicBlock *bb) {
    if (v) {
      v->add_use(this);
    }
    incomings_.emplace_back(std::move(v), std::move(bb));
  }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }

    for (auto &inc : incomings_) {
      if (inc.first.get() == old_op) {
        old_op->remove_use(this);
        new_op->add_use(this);
        inc.first = std::static_pointer_cast<Value>(new_op->shared_from_this());
      }
    }
  }

  void replace_incoming_block(const BasicBlock *old_bb, BasicBlock *new_bb) {
    for (auto &inc : incomings_) {
      if (inc.second == old_bb) {
        inc.second = new_bb;
      }
    }
  }

  void remove_incoming_block(const BasicBlock *bb) {
    if (!bb) {
      return;
    }
    for (auto it = incomings_.begin(); it != incomings_.end();) {
      if (it->second == bb) {
        if (it->first) {
          it->first->remove_use(this);
        }
        it = incomings_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void drop_all_references() override {
    for (auto *op : operands) {
      if (op) {
        op->remove_use(this);
      }
    }
    operands.clear();

    for (auto &inc : incomings_) {
      if (inc.first) {
        inc.first->remove_use(this);
      }
    }
    incomings_.clear();
  }

  std::vector<Incoming> &incomings() { return incomings_; }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap &block_map) const override {
    std::vector<Incoming> incomings;
    incomings.reserve(incomings_.size());
    for (const auto &inc : incomings_) {
      incomings.emplace_back(remap_value(inc.first, value_map),
                             remap_block(inc.second, block_map));
    }
    return std::make_shared<PhiInst>(new_parent, type(), std::move(incomings),
                                     name());
  }

private:
  std::vector<Incoming> incomings_;
};

class SelectInst : public Instruction {
public:
  SelectInst(BasicBlock *parent, std::shared_ptr<Value> cond,
             std::shared_ptr<Value> if_true, std::shared_ptr<Value> if_false,
             TypePtr ty, std::string name = {})
      : Instruction(parent, std::move(ty), std::move(name)),
        cond_(std::move(cond)), if_true_(std::move(if_true)),
        if_false_(std::move(if_false)) {
    if (!cond_) {
      throw std::invalid_argument("SelectInst requires a condition");
    }
    auto i1 = std::dynamic_pointer_cast<const IntegerType>(cond_->type());
    if (!i1 || i1->bits() != 1) {
      throw std::invalid_argument("SelectInst condition must be i1");
    }

    add_operand(cond_);
    add_operand(if_true_);
    add_operand(if_false_);
  }

  const std::shared_ptr<Value> &cond() const { return cond_; }
  const std::shared_ptr<Value> &if_true() const { return if_true_; }
  const std::shared_ptr<Value> &if_false() const { return if_false_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

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
    if (if_true_.get() == old_op) {
      if_true_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
    if (if_false_.get() == old_op) {
      if_false_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<SelectInst>(
        new_parent, remap_value(cond_, value_map), remap_value(if_true_, value_map),
        remap_value(if_false_, value_map), type(), name());
  }

private:
  std::shared_ptr<Value> cond_;
  std::shared_ptr<Value> if_true_;
  std::shared_ptr<Value> if_false_;
};

class ZExtInst : public Instruction {
public:
  ZExtInst(BasicBlock *parent, std::shared_ptr<Value> src, TypePtr dest_ty,
           std::string name = {})
      : Instruction(parent, std::move(dest_ty), std::move(name)),
        src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("ZExtInst source cannot be null");
    }

    add_operand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }
    if (src_.get() == old_op) {
      src_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  TypePtr dest_type() const { return type(); }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<ZExtInst>(new_parent, remap_value(src_, value_map),
                                      dest_type(), name());
  }

private:
  std::shared_ptr<Value> src_;
};

class SExtInst : public Instruction {
public:
  SExtInst(BasicBlock *parent, std::shared_ptr<Value> src, TypePtr dest_ty,
           std::string name = {})
      : Instruction(parent, std::move(dest_ty), std::move(name)),
        src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("SExtInst source cannot be null");
    }

    add_operand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

  size_t src_bits() const {
    auto src_int = std::dynamic_pointer_cast<const IntegerType>(src_->type());
    if (!src_int) {
      throw std::invalid_argument("SExtInst source type must be integer");
    }
    return src_int->bits();
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
    if (src_.get() == old_op) {
      src_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  TypePtr dest_type() const { return type(); }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<SExtInst>(new_parent, remap_value(src_, value_map),
                                      dest_type(), name());
  }

private:
  std::shared_ptr<Value> src_;
};

class TruncInst : public Instruction {
public:
  TruncInst(BasicBlock *parent, std::shared_ptr<Value> src, TypePtr dest_ty,
            std::string name = {})
      : Instruction(parent, std::move(dest_ty), std::move(name)),
        src_(std::move(src)) {
    if (!src_) {
      throw std::invalid_argument("TruncInst source cannot be null");
    }

    add_operand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  TypePtr dest_type() const { return type(); }

  size_t dest_bits() const {
    auto dest_int = std::dynamic_pointer_cast<const IntegerType>(type());
    if (!dest_int) {
      throw std::invalid_argument("TruncInst destination type is not integer");
    }
    return dest_int->bits();
  }

  size_t shift_bits() const {
    auto src_int = std::dynamic_pointer_cast<const IntegerType>(src_->type());
    auto dest_int = std::dynamic_pointer_cast<const IntegerType>(type());
    if (!src_int || !dest_int) {
      throw std::invalid_argument(
          "TruncInst source and destination types must be integer");
    }
    if (src_int->bits() <= dest_int->bits()) {
      throw std::invalid_argument(
          "TruncInst source type must be larger than destination type");
    }
    return src_int->bits() - dest_int->bits();
  }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }
    if (src_.get() == old_op) {
      src_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<TruncInst>(new_parent, remap_value(src_, value_map),
                                       dest_type(), name());
  }

private:
  std::shared_ptr<Value> src_;
};

class MoveInst : public Instruction {
public:
  MoveInst(BasicBlock *parent, std::shared_ptr<Value> src,
           std::shared_ptr<Value> dest, std::string name = {})
      : Instruction(parent, dest->type(), std::move(name)),
        src_(std::move(src)), dest_(std::move(dest)) {
    // NOTE: the dest of a MoveInst is a Phi instruction.
    if (!src_ || !dest_) {
      throw std::invalid_argument(
          "MoveInst source and destination cannot be null");
    }
    add_operand(src_);
  }

  const std::shared_ptr<Value> &source() const { return src_; }
  const std::shared_ptr<Value> &destination() const { return dest_; }

  void accept(InstructionVisitor &v) const override { v.visit(*this); }

  void replace_operand(Value *old_op, Value *new_op) override {
    for (auto &op : operands) {
      if (op == old_op) {
        old_op->remove_use(this);
        op = new_op;
        new_op->add_use(this);
      }
    }
    if (src_.get() == old_op) {
      src_ = std::static_pointer_cast<Value>(new_op->shared_from_this());
    }
  }

  std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap & /*blockMap*/) const override {
    return std::make_shared<MoveInst>(new_parent, remap_value(src_, value_map),
                                      remap_value(dest_, value_map), name());
  }

private:
  std::shared_ptr<Value> src_;
  std::shared_ptr<Value> dest_;
};

} // namespace rc::ir
