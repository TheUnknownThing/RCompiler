#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rc::ir {

// fwd decls
class BasicBlock;
class Function;

enum class TypeKind {
  Void,
  Integer,
  Pointer,
  Array,
  Struct,
  Function,
};

class Type {
public:
  explicit Type(TypeKind k) : kind_(k) {}
  virtual ~Type() = default;

  TypeKind kind() const { return kind_; }

  bool isVoid() const { return kind_ == TypeKind::Void; }

private:
  TypeKind kind_;
};

using TypePtr = std::shared_ptr<const Type>;

class VoidType final : public Type {
public:
  VoidType() : Type(TypeKind::Void) {}
};

class IntegerType final : public Type {
public:
  IntegerType(unsigned bits, bool isSigned)
      : Type(TypeKind::Integer), bits_(bits), signed_(isSigned) {}

  unsigned bits() const { return bits_; }
  bool isSigned() const { return signed_; }

  static std::shared_ptr<IntegerType> i1() {
    return std::make_shared<IntegerType>(1, true);
  }
  static std::shared_ptr<IntegerType> i8(bool s = false) {
    return std::make_shared<IntegerType>(8, s);
  }
  static std::shared_ptr<IntegerType> i32(bool s = true) {
    return std::make_shared<IntegerType>(32, s);
  }

  static std::shared_ptr<IntegerType> isize() {
    return std::make_shared<IntegerType>(32, true);
  }
  static std::shared_ptr<IntegerType> usize() {
    return std::make_shared<IntegerType>(32, false);
  }

private:
  unsigned bits_;
  bool signed_;
};

class PointerType final : public Type {
public:
  explicit PointerType(TypePtr pointee)
      : Type(TypeKind::Pointer), pointee_(std::move(pointee)) {}
  const TypePtr &pointee() const { return pointee_; }

private:
  TypePtr pointee_;
};

class ArrayType final : public Type {
public:
  ArrayType(TypePtr elem, std::size_t count)
      : Type(TypeKind::Array), elem_(std::move(elem)), count_(count) {}
  const TypePtr &elem() const { return elem_; }
  std::size_t count() const { return count_; }

private:
  TypePtr elem_;
  std::size_t count_;
};

class StructType final : public Type {
public:
  explicit StructType(std::vector<TypePtr> fields, std::string name = {})
      : Type(TypeKind::Struct), fields_(std::move(fields)),
        name_(std::move(name)) {}
  const std::vector<TypePtr> &fields() const { return fields_; }
  const std::string &name() const { return name_; }
  bool isEmpty() const { return fields_.empty(); }

private:
  std::vector<TypePtr> fields_;
  std::string name_;
};

class FunctionType final : public Type {
public:
  FunctionType(TypePtr retTy, std::vector<TypePtr> paramTys,
               bool isVarArg = false,
               std::shared_ptr<Function> function = nullptr)
      : Type(TypeKind::Function), ret_(std::move(retTy)),
        params_(std::move(paramTys)), varArg_(isVarArg),
        function_(std::move(function)) {}
  const TypePtr &returnType() const { return ret_; }
  const std::vector<TypePtr> &paramTypes() const { return params_; }
  bool isVarArg() const { return varArg_; }
  const std::shared_ptr<Function> &function() const { return function_; }
  void setFunction(std::shared_ptr<Function> fn) { function_ = std::move(fn); }

private:
  TypePtr ret_;
  std::vector<TypePtr> params_;
  bool varArg_;
  std::shared_ptr<Function> function_;
};

class Instruction;

class Value : public std::enable_shared_from_this<Value> {
public:
  explicit Value(TypePtr ty, std::string name = {})
      : type_(std::move(ty)), name_(std::move(name)) {}
  virtual ~Value() = default;

  const TypePtr &type() const { return type_; }
  const std::string &name() const { return name_; }
  void setName(std::string n) { name_ = std::move(n); }
  void addUse(Instruction *ins) { use_list.push_back(ins); }
  void removeUse(Instruction *ins) { use_list.remove(ins); }
  std::list<Instruction *> &getUses() { return use_list; }
  const std::list<Instruction *> &getUses() const { return use_list; }

protected:
  void setType(TypePtr t) { type_ = std::move(t); }

private:
  TypePtr type_;
  std::string name_; // register name
  std::list<Instruction *> use_list;
};

class Constant : public Value {
public:
  using Value::Value;

  virtual ~Constant() override = default;

  virtual bool equals(const Constant &other) const = 0;
};

using ValueRemapMap = std::unordered_map<Value *, std::shared_ptr<Value>>;
using BlockRemapMap =
    std::unordered_map<BasicBlock *, std::shared_ptr<BasicBlock>>;

inline std::shared_ptr<Value> remapValue(const std::shared_ptr<Value> &v,
                                         const ValueRemapMap &valueMap) {
  if (!v) {
    return nullptr;
  }
  auto it = valueMap.find(v.get());
  if (it != valueMap.end()) {
    return it->second;
  }
  return v;
}

inline std::shared_ptr<BasicBlock>
remapBlock(const std::shared_ptr<BasicBlock> &bb,
           const BlockRemapMap &blockMap) {
  if (!bb) {
    return nullptr;
  }
  auto it = blockMap.find(bb.get());
  if (it != blockMap.end()) {
    return it->second;
  }
  return bb;
}

class Instruction : public Value {
public:
  explicit Instruction(BasicBlock *parent, TypePtr ty, std::string name = {})
      : Value(std::move(ty), std::move(name)), parent_(parent) {}

  void addOperands(std::vector<Value *> ops) {
    operands.insert(operands.end(), ops.begin(), ops.end());
    for (auto *op : ops) {
      op->addUse(this);
    }
  }
  void addOperands(const std::vector<std::shared_ptr<Value>> &ops) {
    for (const auto &op : ops) {
      operands.push_back(op.get());
      op->addUse(this);
    }
  }
  void addOperand(Value *op) {
    operands.push_back(op);
    op->addUse(this);
  }
  void addOperand(const std::shared_ptr<Value> &op) {
    operands.push_back(op.get());
    op->addUse(this);
  }

  std::vector<Value *> &getOperands() { return operands; }
  const std::vector<Value *> &getOperands() const { return operands; }

  virtual void replaceOperand(Value *oldOp, Value *newOp) = 0;

  virtual std::shared_ptr<Instruction>
  cloneInst(BasicBlock *newParent, const ValueRemapMap &valueMap,
            const BlockRemapMap &blockMap) const = 0;

  virtual void dropAllReferences() {
    for (auto *op : operands) {
      if (op) {
        op->removeUse(this);
      }
    }
    operands.clear();
  }

  BasicBlock *parent() const { return parent_; }
  void setParent(BasicBlock *p) { parent_ = p; }

  Instruction *next() const { return next_; }
  void setNext(Instruction *next) { next_ = next; }
  Instruction *prev() const { return prev_; }
  void setPrev(Instruction *prev) { prev_ = prev; }

  using Value::Value;
  ~Instruction() override = default;

protected:
  std::vector<Value *> operands;

private:
  BasicBlock *parent_{nullptr};
  Instruction *next_{nullptr}, *prev_{nullptr};
};

class ConstantInt final : public Constant {
public:
  ConstantInt(std::shared_ptr<IntegerType> ty, std::uint64_t v)
      : Constant(std::move(ty)), value_(v) {}

  std::uint64_t value() const { return value_; }

  static std::shared_ptr<ConstantInt> getI1(bool v) {
    return std::make_shared<ConstantInt>(IntegerType::i1(), v ? 1ULL : 0ULL);
  }
  static std::shared_ptr<ConstantInt> getI32(std::uint32_t v,
                                             bool isSigned = true) {
    return std::make_shared<ConstantInt>(IntegerType::i32(isSigned), v);
  }

  bool equals(const Constant &other) const override {
    if (auto otherInt = dynamic_cast<const ConstantInt *>(&other)) {
      return value_ == otherInt->value();
    }
    return false;
  }

private:
  std::uint64_t value_;
};

class ConstantUnit final : public Constant {
public:
  ConstantUnit() : Constant(std::make_shared<VoidType>()) {}

  bool equals(const Constant &other) const override {
    return dynamic_cast<const ConstantUnit *>(&other) != nullptr;
  }
};

class ConstantNull final : public Constant {
public:
  explicit ConstantNull(TypePtr ptrTy) : Constant(std::move(ptrTy)) {}

  bool equals(const Constant &other) const override {
    return dynamic_cast<const ConstantNull *>(&other) != nullptr;
  }
};

class UndefValue final : public Constant {
public:
  explicit UndefValue(TypePtr ty) : Constant(std::move(ty)) {}

  bool equals(const Constant &other) const override {
    return dynamic_cast<const UndefValue *>(&other) != nullptr;
  }
};

class ConstantString final : public Constant {
public:
  explicit ConstantString(std::string data)
      : Constant(nullptr), data_(std::move(data)),
        arrayType_(
            std::make_shared<ArrayType>(IntegerType::i8(false), data_.size())) {
    setType(std::make_shared<PointerType>(IntegerType::i8(false)));
  }

  const std::string &data() const { return data_; }
  TypePtr arrayType() const { return arrayType_; }

  bool equals(const Constant &other) const override {
    if (auto otherStr = dynamic_cast<const ConstantString *>(&other)) {
      return data_ == otherStr->data();
    }
    return false;
  }

private:
  std::string data_;
  TypePtr arrayType_;
};

class ConstantPtr final : public Constant {
public:
  ConstantPtr(TypePtr ptrTy, std::shared_ptr<Constant> pointee)
      : Constant(std::move(ptrTy)), pointee_(std::move(pointee)) {}
  const std::shared_ptr<Constant> &pointee() const { return pointee_; }

  bool equals(const Constant &other) const override {
    if (auto otherPtr = dynamic_cast<const ConstantPtr *>(&other)) {
      return pointee_->equals(*otherPtr->pointee());
    }
    return false;
  }

private:
  std::shared_ptr<Constant> pointee_;
};

class ConstantArray final : public Constant {
public:
  ConstantArray(TypePtr elemTy, std::vector<std::shared_ptr<Constant>> elems)
      : Constant(nullptr), elements_(std::move(elems)),
        arrayType_(
            std::make_shared<ArrayType>(std::move(elemTy), elements_.size())) {
    setType(std::make_shared<PointerType>(arrayType_));
  }

  const std::vector<std::shared_ptr<Constant>> &elements() const {
    return elements_;
  }
  TypePtr arrayType() const { return arrayType_; }

  bool equals(const Constant &other) const override {
    auto otherArr = dynamic_cast<const ConstantArray *>(&other);
    if (!otherArr)
      return false;
    if (elements_.size() != otherArr->elements_.size())
      return false;
    for (size_t i = 0; i < elements_.size(); ++i) {
      if (!elements_[i]->equals(*otherArr->elements_[i]))
        return false;
    }
    return true;
  }

private:
  std::vector<std::shared_ptr<Constant>> elements_;
  TypePtr arrayType_;
};

} // namespace rc::ir
