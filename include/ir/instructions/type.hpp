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

  bool is_void() const { return kind_ == TypeKind::Void; }

private:
  TypeKind kind_;
};

using TypePtr = std::shared_ptr<const Type>;

class VoidType final : public Type {
public:
  VoidType() : Type(TypeKind::Void) {}
  static std::shared_ptr<VoidType> get() {
    return std::make_shared<VoidType>();
  }
};

class IntegerType final : public Type {
public:
  IntegerType(unsigned bits, bool is_signed)
      : Type(TypeKind::Integer), bits_(bits), signed_(is_signed) {}

  unsigned bits() const { return bits_; }
  bool is_signed() const { return signed_; }

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
  bool is_empty() const { return fields_.empty(); }

private:
  std::vector<TypePtr> fields_;
  std::string name_;
};

class FunctionType final : public Type {
public:
  FunctionType(TypePtr ret_ty, std::vector<TypePtr> param_tys,
               bool is_var_arg = false,
               Function *function = nullptr)
      : Type(TypeKind::Function), ret_(std::move(ret_ty)),
        params_(std::move(param_tys)), var_arg_(is_var_arg),
        function_(std::move(function)) {}
  const TypePtr &return_type() const { return ret_; }
  const std::vector<TypePtr> &param_types() const { return params_; }
  bool is_var_arg() const { return var_arg_; }
  Function *function() const { return function_; }
  void set_function(Function *fn) { function_ = fn; }

private:
  TypePtr ret_;
  std::vector<TypePtr> params_;
  bool var_arg_;
  Function *function_;
};

class Instruction;
struct InstructionVisitor;

class Value : public std::enable_shared_from_this<Value> {
public:
  explicit Value(TypePtr ty, std::string name = {})
      : type_(std::move(ty)), name_(std::move(name)) {}
  virtual ~Value() = default;

  const TypePtr &type() const { return type_; }
  const std::string &name() const { return name_; }
  void set_name(std::string n) { name_ = std::move(n); }
  void add_use(Instruction *ins) { use_list.push_back(ins); }
  void remove_use(Instruction *ins) { use_list.remove(ins); }
  std::list<Instruction *> &get_uses() { return use_list; }
  const std::list<Instruction *> &get_uses() const { return use_list; }

protected:
  void set_type(TypePtr t) { type_ = std::move(t); }

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
  std::unordered_map<BasicBlock *, BasicBlock *>;

std::shared_ptr<Value> remap_value(const std::shared_ptr<Value> &v,
                                         const ValueRemapMap &value_map);

BasicBlock *remap_block(BasicBlock *bb, const BlockRemapMap &block_map);

class Instruction : public Value {
public:
  explicit Instruction(BasicBlock *parent, TypePtr ty, std::string name = {})
      : Value(std::move(ty), std::move(name)), parent_(parent) {}

  void add_operands(std::vector<Value *> ops) {
    operands.insert(operands.end(), ops.begin(), ops.end());
    for (auto *op : ops) {
      op->add_use(this);
    }
  }
  void add_operands(const std::vector<std::shared_ptr<Value>> &ops) {
    for (const auto &op : ops) {
      operands.push_back(op.get());
      op->add_use(this);
    }
  }
  void add_operand(Value *op) {
    operands.push_back(op);
    op->add_use(this);
  }
  void add_operand(const std::shared_ptr<Value> &op) {
    operands.push_back(op.get());
    op->add_use(this);
  }

  std::vector<Value *> &get_operands() { return operands; }
  const std::vector<Value *> &get_operands() const { return operands; }

  virtual void accept(InstructionVisitor &v) const = 0;
  virtual bool is_terminator() const { return false; }

  virtual void replace_operand(Value *old_op, Value *new_op) = 0;

  virtual std::shared_ptr<Instruction>
  clone_inst(BasicBlock *new_parent, const ValueRemapMap &value_map,
            const BlockRemapMap &block_map) const = 0;

  virtual void drop_all_references() {
    for (auto *op : operands) {
      if (op) {
        op->remove_use(this);
      }
    }
    operands.clear();
  }

  BasicBlock *parent() const { return parent_; }
  void set_parent(BasicBlock *p) { parent_ = p; }

  Instruction *next() const { return next_; }
  void set_next(Instruction *next) { next_ = next; }
  Instruction *prev() const { return prev_; }
  void set_prev(Instruction *prev) { prev_ = prev; }

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

  static std::shared_ptr<ConstantInt> get_i1(bool v) {
    return std::make_shared<ConstantInt>(IntegerType::i1(), v ? 1ULL : 0ULL);
  }
  static std::shared_ptr<ConstantInt> get_i32(std::uint32_t v,
                                             bool is_signed = true) {
    return std::make_shared<ConstantInt>(IntegerType::i32(is_signed), v);
  }

  bool equals(const Constant &other) const override {
    if (auto other_int = dynamic_cast<const ConstantInt *>(&other)) {
      return value_ == other_int->value();
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
  explicit ConstantNull(TypePtr ptr_ty) : Constant(std::move(ptr_ty)) {}

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
        array_type_(
            std::make_shared<ArrayType>(IntegerType::i8(false), data_.size())) {
    set_type(std::make_shared<PointerType>(IntegerType::i8(false)));
  }

  const std::string &data() const { return data_; }
  TypePtr array_type() const { return array_type_; }

  bool equals(const Constant &other) const override {
    if (auto other_str = dynamic_cast<const ConstantString *>(&other)) {
      return data_ == other_str->data();
    }
    return false;
  }

private:
  std::string data_;
  TypePtr array_type_;
};

class ConstantPtr final : public Constant {
public:
  ConstantPtr(TypePtr ptr_ty, std::shared_ptr<Constant> pointee)
      : Constant(std::move(ptr_ty)), pointee_(std::move(pointee)) {}
  const std::shared_ptr<Constant> &pointee() const { return pointee_; }

  bool equals(const Constant &other) const override {
    if (auto other_ptr = dynamic_cast<const ConstantPtr *>(&other)) {
      return pointee_->equals(*other_ptr->pointee());
    }
    return false;
  }

private:
  std::shared_ptr<Constant> pointee_;
};

class ConstantArray final : public Constant {
public:
  ConstantArray(TypePtr elem_ty, std::vector<std::shared_ptr<Constant>> elems)
      : Constant(nullptr), elements_(std::move(elems)),
        array_type_(
            std::make_shared<ArrayType>(std::move(elem_ty), elements_.size())) {
    set_type(std::make_shared<PointerType>(array_type_));
  }

  const std::vector<std::shared_ptr<Constant>> &elements() const {
    return elements_;
  }
  TypePtr array_type() const { return array_type_; }

  bool equals(const Constant &other) const override {
    auto other_arr = dynamic_cast<const ConstantArray *>(&other);
    if (!other_arr)
      return false;
    if (elements_.size() != other_arr->elements_.size())
      return false;
    for (size_t i = 0; i < elements_.size(); ++i) {
      if (!elements_[i]->equals(*other_arr->elements_[i]))
        return false;
    }
    return true;
  }

private:
  std::vector<std::shared_ptr<Constant>> elements_;
  TypePtr array_type_;
};

} // namespace rc::ir
