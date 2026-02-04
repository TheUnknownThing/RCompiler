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
  void setName(std::string n) { name_ = std::move(n); }

  template <class T, class... Args> std::shared_ptr<T> append(Args &&...args) {
    auto inst = std::make_shared<T>(this, std::forward<Args>(args)...);
    std::static_pointer_cast<Instruction>(inst)->setPrev(
        instructions_.empty() ? nullptr : instructions_.back().get());
    if (!instructions_.empty()) {
      instructions_.back()->setNext(inst.get());
    }
    std::static_pointer_cast<Instruction>(inst)->setNext(nullptr);
    instructions_.push_back(inst);
    return inst;
  }

  template <class T, class... Args> std::shared_ptr<T> prepend(Args &&...args) {
    auto inst = std::make_shared<T>(this, std::forward<Args>(args)...);
    instructions_.insert(instructions_.begin() + prologue_insert_pos_, inst);
    if (prologue_insert_pos_ > 0) {
      instructions_[prologue_insert_pos_ - 1]->setNext(inst.get());
      std::static_pointer_cast<Instruction>(inst)->setPrev(
          instructions_[prologue_insert_pos_ - 1].get());
    } else {
      std::static_pointer_cast<Instruction>(inst)->setPrev(nullptr);
    }
    if (prologue_insert_pos_ < instructions_.size() - 1) {
      instructions_[prologue_insert_pos_ + 1]->setPrev(inst.get());
      std::static_pointer_cast<Instruction>(inst)->setNext(
          instructions_[prologue_insert_pos_ + 1].get());
    } else {
      std::static_pointer_cast<Instruction>(inst)->setNext(nullptr);
    }
    ++prologue_insert_pos_;
    return inst;
  }

  std::vector<std::shared_ptr<Instruction>> &instructions() {
    return instructions_;
  }

  const std::vector<std::shared_ptr<Instruction>> &instructions() const {
    return instructions_;
  }

  bool isTerminated() const;

  Function *parent() const { return parent_; }

  void setParent(Function *p) { parent_ = p; }

  void addPredecessor(BasicBlock *bb) { predecessors_.push_back(bb); }

  void removePredecessor(BasicBlock *bb) {
    predecessors_.erase(
        std::remove(predecessors_.begin(), predecessors_.end(), bb),
        predecessors_.end());
  }

  void clearPredecessors() { predecessors_.clear(); }

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
  Function(std::string name, std::shared_ptr<FunctionType> fnTy,
           bool isExternal = false)
      : name_(std::move(name)), type_(std::move(fnTy)),
        isExternal_(isExternal) {
    // Pre-create Argument nodes for each parameter
    unsigned i = 0;
    for (const auto &pt : type_->paramTypes()) {
      (void)pt;
      args_.push_back(
          std::make_shared<Argument>(type_->paramTypes()[i], "", i));
      ++i;
    }
  }

  const std::string &name() const { return name_; }
  void setName(std::string n) { name_ = std::move(n); }

  const std::shared_ptr<FunctionType> &type() const { return type_; }
  bool isExternal() const { return isExternal_; }

  const std::vector<std::shared_ptr<Argument>> &args() const { return args_; }

  std::shared_ptr<BasicBlock> createBlock(std::string label) {
    auto bb = std::make_shared<BasicBlock>(std::move(label), this);
    blocks_.push_back(bb);
    return bb;
  }

  void appendBlock(std::shared_ptr<BasicBlock> bb) {
    blocks_.push_back(std::move(bb));
  }

  std::vector<std::shared_ptr<BasicBlock>> &blocks() { return blocks_; }

  const std::vector<std::shared_ptr<BasicBlock>> &blocks() const {
    return blocks_;
  }

  std::shared_ptr<BasicBlock> splitBlock(std::shared_ptr<BasicBlock> bb,
                                         Instruction *inst) {
    auto newBB = std::make_shared<BasicBlock>(bb->name() + "_split", this);
    appendBlock(newBB);
    auto &insts = bb->instructions();
    auto it = std::find_if(insts.begin(), insts.end(),
                           [inst](const std::shared_ptr<Instruction> &i) {
                             return i.get() == inst;
                           });
    if (it + 1 != insts.end()) {
      newBB->instructions().insert(newBB->instructions().end(), it + 1,
                                   insts.end());
      insts.erase(it + 1, insts.end());
      return newBB;
    }
    return nullptr;
  }

  std::vector<std::shared_ptr<Argument>> &params() { return args_; }

  const std::vector<std::shared_ptr<Argument>> &params() const { return args_; }

private:
  std::string name_;
  std::shared_ptr<FunctionType> type_;
  bool isExternal_;
  std::vector<std::shared_ptr<Argument>> args_;
  std::vector<std::shared_ptr<BasicBlock>> blocks_;
};

struct TargetInfo {
  std::string triple;        // e.g., x86_64-apple-darwin
  std::string dataLayout;    // LLVM datalayout string (optional)
  unsigned pointerWidth{64}; // in bits; used for isize/usize mapping
};

class Module : public std::enable_shared_from_this<Module> {
public:
  explicit Module(std::string name, TargetInfo target = {})
      : name_(std::move(name)), target_(std::move(target)) {}

  const std::string &name() const { return name_; }
  const TargetInfo &target() const { return target_; }
  TargetInfo &target() { return target_; }

  std::shared_ptr<Function> createFunction(const std::string &name,
                                           std::shared_ptr<FunctionType> fnTy,
                                           bool isExternal = false) {
    auto fn = std::make_shared<Function>(name, std::move(fnTy), isExternal);
    if (fn->type()) {
      fn->type()->setFunction(fn);
    }
    functions_.push_back(fn);
    return fn;
  }

  std::shared_ptr<StructType>
  createStructType(const std::vector<std::pair<std::string, TypePtr>> &fields,
                   const std::string &name = {}) {
    std::vector<TypePtr> fieldTypes;
    fieldTypes.reserve(fields.size());
    for (const auto &f : fields) {
      fieldTypes.push_back(f.second);
    }
    auto st = std::make_shared<StructType>(fieldTypes, name);
    struct_types_.push_back(st);
    return st;
  }

  std::shared_ptr<Constant> createConstant(std::shared_ptr<Constant> constant) {
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
