#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "type.hpp"

namespace rc::ir {

class BinaryOpInst;
class BranchInst;
class ReturnInst;
class AllocaInst;
class LoadInst;
class StoreInst;
class GetElementPtrInst;
class ICmpInst;
class CallInst;
class PhiInst;
class SelectInst;

class Argument : public Value {
public:
  Argument(TypePtr ty, std::string name = {}, unsigned index = 0)
      : Value(std::move(ty), std::move(name)), index_(index) {}
  unsigned index() const { return index_; }

private:
  unsigned index_;
};

class BasicBlock : public std::enable_shared_from_this<BasicBlock> {
public:
  explicit BasicBlock(std::string name = {}, Function *parent = nullptr)
      : name_(std::move(name)), parent_(parent) {}

  const std::string &name() const { return name_; }
  void set_name(std::string n) { name_ = std::move(n); }

  template <class T, class... Args> std::shared_ptr<T> append(Args &&...args) {
    auto inst = std::make_shared<T>(this, std::forward<Args>(args)...);
    std::static_pointer_cast<Instruction>(inst)->set_prev(
        instructions_.empty() ? nullptr : instructions_.back().get());
    if (!instructions_.empty()) {
      instructions_.back()->set_next(inst.get());
    }
    std::static_pointer_cast<Instruction>(inst)->set_next(nullptr);
    instructions_.push_back(inst);
    return inst;
  }

  template <class T, class... Args> std::shared_ptr<T> prepend(Args &&...args) {
    auto inst = std::make_shared<T>(this, std::forward<Args>(args)...);
    instructions_.insert(instructions_.begin() + prologue_insert_pos_, inst);
    if (prologue_insert_pos_ > 0) {
      instructions_[prologue_insert_pos_ - 1]->set_next(inst.get());
      std::static_pointer_cast<Instruction>(inst)->set_prev(
          instructions_[prologue_insert_pos_ - 1].get());
    } else {
      std::static_pointer_cast<Instruction>(inst)->set_prev(nullptr);
    }
    if (prologue_insert_pos_ < instructions_.size() - 1) {
      instructions_[prologue_insert_pos_ + 1]->set_prev(inst.get());
      std::static_pointer_cast<Instruction>(inst)->set_next(
          instructions_[prologue_insert_pos_ + 1].get());
    } else {
      std::static_pointer_cast<Instruction>(inst)->set_next(nullptr);
    }
    ++prologue_insert_pos_;
    return inst;
  }

  template <class T, class... Args>
  std::shared_ptr<T> insert_before(const std::shared_ptr<Instruction> &pos,
                                  Args &&...args) {
    auto it = std::find(instructions_.begin(), instructions_.end(), pos);
    if (it == instructions_.end()) {
      throw std::invalid_argument("Position instruction not found in block");
    }
    auto inst = std::make_shared<T>(this, std::forward<Args>(args)...);
    if (it != instructions_.begin()) {
      (*(it - 1))->set_next(inst.get());
      std::static_pointer_cast<Instruction>(inst)->set_prev((*(it - 1)).get());
    } else {
      std::static_pointer_cast<Instruction>(inst)->set_prev(nullptr);
    }
    (*it)->set_prev(inst.get());
    std::static_pointer_cast<Instruction>(inst)->set_next((*it).get());
    instructions_.insert(it, inst);
    return inst;
  }

  std::vector<std::shared_ptr<Instruction>> &instructions() {
    return instructions_;
  }

  const std::vector<std::shared_ptr<Instruction>> &instructions() const {
    return instructions_;
  }

  void erase_instruction(const std::shared_ptr<Instruction> &inst) {
    auto it = std::find(instructions_.begin(), instructions_.end(), inst);
    if (it != instructions_.end()) {
      (*it)->drop_all_references();
      if (it != instructions_.begin()) {
        (*(it - 1))->set_next((it + 1) != instructions_.end() ? (*(it + 1)).get()
                                                             : nullptr);
      }
      if ((it + 1) != instructions_.end()) {
        (*(it + 1))->set_prev(it != instructions_.begin() ? (*(it - 1)).get()
                                                         : nullptr);
      }
      instructions_.erase(it);
      if (prologue_insert_pos_ > 0) {
        --prologue_insert_pos_;
      }
    }
  }

  bool is_terminated() const;

  Function *parent() const { return parent_; }

  void set_parent(Function *p) { parent_ = p; }

  void add_predecessor(BasicBlock *bb) { predecessors_.push_back(bb); }

  void remove_predecessor(BasicBlock *bb) {
    predecessors_.erase(
        std::remove(predecessors_.begin(), predecessors_.end(), bb),
        predecessors_.end());
  }

  void replace_predecessor(BasicBlock *old_bb, BasicBlock *new_bb) {
    std::replace(predecessors_.begin(), predecessors_.end(), old_bb, new_bb);
  }

  void clear_predecessors() { predecessors_.clear(); }

  const std::vector<BasicBlock *> &predecessors() const {
    return predecessors_;
  }

private:
  std::string name_;
  std::vector<std::shared_ptr<Instruction>> instructions_;
  std::size_t prologue_insert_pos_{0};
  Function *parent_{nullptr};
  std::vector<BasicBlock *> predecessors_;
};

class Function : public std::enable_shared_from_this<Function> {
public:
  Function(std::string name, std::shared_ptr<FunctionType> fn_ty,
           bool is_external = false)
      : name_(std::move(name)), type_(std::move(fn_ty)),
        is_external_(is_external) {
    // Pre-create Argument nodes for each parameter
    unsigned i = 0;
    for (const auto &pt : type_->param_types()) {
      (void)pt;
      args_.push_back(
          std::make_shared<Argument>(type_->param_types()[i], "", i));
      ++i;
    }
  }

  const std::string &name() const { return name_; }
  void set_name(std::string n) { name_ = std::move(n); }

  const std::shared_ptr<FunctionType> &type() const { return type_; }
  bool is_external() const { return is_external_; }

  const std::vector<std::shared_ptr<Argument>> &args() const { return args_; }

  std::shared_ptr<BasicBlock> create_block(std::string label) {
    auto bb = std::make_shared<BasicBlock>(std::move(label), this);
    blocks_.push_back(bb);
    return bb;
  }

  void append_block(std::shared_ptr<BasicBlock> bb) {
    blocks_.push_back(std::move(bb));
  }

  std::vector<std::shared_ptr<BasicBlock>> &blocks() { return blocks_; }

  const std::vector<std::shared_ptr<BasicBlock>> &blocks() const {
    return blocks_;
  }

  std::shared_ptr<BasicBlock> split_block(std::shared_ptr<BasicBlock> bb,
                                         Instruction *inst) {
    auto new_bb = std::make_shared<BasicBlock>(bb->name() + "_split", this);
    append_block(new_bb);
    auto &insts = bb->instructions();
    auto it = std::find_if(insts.begin(), insts.end(),
                           [inst](const std::shared_ptr<Instruction> &i) {
                             return i.get() == inst;
                           });
    if (it != insts.end() && it + 1 != insts.end()) {
      new_bb->instructions().insert(new_bb->instructions().end(), it + 1,
                                   insts.end());
      insts.erase(it + 1, insts.end());

      for (auto &moved : new_bb->instructions()) {
        if (moved) {
          moved->set_parent(new_bb.get());
        }
      }
      for (std::size_t i = 0; i < insts.size(); ++i) {
        auto &cur = insts[i];
        if (!cur) {
          continue;
        }
        cur->set_prev(i == 0 ? nullptr : insts[i - 1].get());
        cur->set_next((i + 1) < insts.size() ? insts[i + 1].get() : nullptr);
      }
      for (std::size_t i = 0; i < new_bb->instructions().size(); ++i) {
        auto &cur = new_bb->instructions()[i];
        if (!cur) {
          continue;
        }
        cur->set_prev(i == 0 ? nullptr : new_bb->instructions()[i - 1].get());
        cur->set_next((i + 1) < new_bb->instructions().size()
                         ? new_bb->instructions()[i + 1].get()
                         : nullptr);
      }
      return new_bb;
    }
    return nullptr;
  }

  void erase_block(std::shared_ptr<BasicBlock> bb) {
    auto it = std::find(blocks_.begin(), blocks_.end(), bb);
    if (it != blocks_.end()) {
      for (auto &inst : (*it)->instructions()) {
        if (inst) {
          inst->drop_all_references();
        }
      }
      (*it)->instructions().clear();
      blocks_.erase(it);
    }
  }

  std::vector<std::shared_ptr<Argument>> &params() { return args_; }

  const std::vector<std::shared_ptr<Argument>> &params() const { return args_; }

  TypePtr return_type() const {
    if (type_) {
      return type_->return_type();
    }
    return nullptr;
  }

private:
  std::string name_;
  std::shared_ptr<FunctionType> type_;
  bool is_external_;
  std::vector<std::shared_ptr<Argument>> args_;
  std::vector<std::shared_ptr<BasicBlock>> blocks_;
};

struct TargetInfo {
  std::string triple;        // e.g., x86_64-apple-darwin
  std::string data_layout;    // LLVM datalayout string (optional)
  unsigned pointer_width{32}; // in bits; used for isize/usize mapping
};

class Module : public std::enable_shared_from_this<Module> {
public:
  explicit Module(std::string name, TargetInfo target = {})
      : name_(std::move(name)), target_(std::move(target)) {}

  const std::string &name() const { return name_; }
  const TargetInfo &target() const { return target_; }
  TargetInfo &target() { return target_; }

  std::shared_ptr<Function> create_function(const std::string &name,
                                           std::shared_ptr<FunctionType> fn_ty,
                                           bool is_external = false) {
    auto fn = std::make_shared<Function>(name, std::move(fn_ty), is_external);
    if (fn->type()) {
      fn->type()->set_function(fn.get());
    }
    functions_.push_back(fn);
    return fn;
  }

  std::shared_ptr<StructType>
  create_struct_type(const std::vector<std::pair<std::string, TypePtr>> &fields,
                   const std::string &name = {}) {
    std::vector<TypePtr> field_types;
    field_types.reserve(fields.size());
    for (const auto &f : fields) {
      field_types.push_back(f.second);
    }
    auto st = std::make_shared<StructType>(field_types, name);
    struct_types_.push_back(st);
    return st;
  }

  std::shared_ptr<Constant> create_constant(std::shared_ptr<Constant> constant) {
    constants_.push_back(constant);
    return constant;
  }

  const std::vector<std::shared_ptr<Function>> &functions() const {
    return functions_;
  }

  const std::vector<std::shared_ptr<StructType>> &struct_types() const {
    return struct_types_;
  }

  const std::vector<std::shared_ptr<Constant>> &constants() const {
    return constants_;
  }

private:
  std::string name_;
  TargetInfo target_;
  std::vector<std::shared_ptr<Function>> functions_;
  std::vector<std::shared_ptr<StructType>> struct_types_;
  std::vector<std::shared_ptr<Constant>> constants_;
};

} // namespace rc::ir
