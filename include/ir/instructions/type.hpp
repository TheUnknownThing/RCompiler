#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rc::ir {

enum class TypeKind {
  Void,
  UnitZst,
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
  bool isUnitZst() const { return kind_ == TypeKind::UnitZst; }

private:
  TypeKind kind_;
};

using TypePtr = std::shared_ptr<const Type>;

class VoidType final : public Type {
public:
  VoidType() : Type(TypeKind::Void) {}
};

class UnitZstType final : public Type {
public:
  UnitZstType() : Type(TypeKind::UnitZst) {}
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
               bool isVarArg = false)
      : Type(TypeKind::Function), ret_(std::move(retTy)),
        params_(std::move(paramTys)), varArg_(isVarArg) {}
  const TypePtr &returnType() const { return ret_; }
  const std::vector<TypePtr> &paramTypes() const { return params_; }
  bool isVarArg() const { return varArg_; }

private:
  TypePtr ret_;
  std::vector<TypePtr> params_;
  bool varArg_;
};

class Value {
public:
  explicit Value(TypePtr ty, std::string name = {})
      : type_(std::move(ty)), name_(std::move(name)) {}
  virtual ~Value() = default;

  const TypePtr &type() const { return type_; }
  const std::string &name() const { return name_; }
  void setName(std::string n) { name_ = std::move(n); }

protected:
  void setType(TypePtr t) { type_ = std::move(t); }

private:
  TypePtr type_;
  std::string name_; // register name
};

class Constant : public Value {
public:
  using Value::Value;
};

class Instruction : public Value {
public:
  using Value::Value;
  ~Instruction() override = default;
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

private:
  std::uint64_t value_;
};

class ConstantUnit final : public Constant {
public:
  ConstantUnit() : Constant(std::make_shared<UnitZstType>()) {}
};

class ConstantNull final : public Constant {
public:
  explicit ConstantNull(TypePtr ptrTy) : Constant(std::move(ptrTy)) {}
};

// fwd decls
class BasicBlock;
class Function;

} // namespace rc::ir