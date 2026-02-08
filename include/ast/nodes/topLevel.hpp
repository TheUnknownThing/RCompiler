#pragma once

#include "../types.hpp"
#include "base.hpp"
#include "expr.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace rc {
class BaseItem : public BaseNode {
public:
  virtual ~BaseItem() = default;
  virtual void accept(class BaseVisitor &visitor) = 0;
};

struct SelfParam {
  bool is_reference = false;
  bool is_mutable = false;
  std::optional<AstType> explicit_type;

  SelfParam(bool ref = false, bool mut_val = false,
            std::optional<AstType> ty = std::nullopt)
      : is_reference(ref), is_mutable(mut_val), explicit_type(ty) {}
};

class FunctionDecl : public BaseItem {
public:
  std::string name;
  std::optional<SelfParam> self_param;
  std::optional<
      std::vector<std::pair<std::shared_ptr<BasePattern>, AstType>>>
      params;
  AstType return_type;
  std::optional<std::shared_ptr<Expression>>
      body; // BlockExpression or semicolon

  FunctionDecl(
      const std::string &nameTok, const std::optional<SelfParam> &self_param,
      const std::optional<
          std::vector<std::pair<std::shared_ptr<BasePattern>, AstType>>>
          &params,
      AstType return_type,
      std::optional<std::shared_ptr<Expression>> body = std::nullopt)
      : name(nameTok), self_param(self_param), params(params),
        return_type(return_type), body(std::move(body)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ConstantItem : public BaseItem {
public:
  std::string name;
  AstType type;
  std::optional<std::shared_ptr<Expression>> value;

  ConstantItem(const std::string &n, AstType t,
               std::optional<std::shared_ptr<Expression>> val = std::nullopt)
      : name(n), type(t), value(std::move(val)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class StructDecl : public BaseItem {
public:
  enum class StructType { Struct, Tuple };

  std::string name;
  StructType struct_type;
  std::vector<std::pair<std::string, AstType>> fields; // For regular struct
  std::vector<AstType> tuple_fields;                   // For tuple struct

  StructDecl(const std::string &n, StructType t,
             std::vector<std::pair<std::string, AstType>> f = {},
             std::vector<AstType> tf = {})
      : name(n), struct_type(t), fields(std::move(f)),
        tuple_fields(std::move(tf)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class EnumDecl : public BaseItem {
public:
  struct EnumVariant {
    std::string name;
  };

  std::string name;
  std::vector<EnumVariant> variants;

  EnumDecl(const std::string &n, std::vector<EnumVariant> vars)
      : name(n), variants(std::move(vars)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class TraitDecl : public BaseItem {
public:
  std::string name;
  std::vector<std::shared_ptr<BaseItem>> associated_items;

  TraitDecl(const std::string &n, std::vector<std::shared_ptr<BaseItem>> items)
      : name(n), associated_items(std::move(items)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ImplDecl : public BaseItem {
public:
  enum class ImplType { Inherent, Trait };

  ImplType impl_type;
  AstType target_type;
  std::optional<std::string> trait_name; // For trait impl
  std::vector<std::shared_ptr<BaseItem>> associated_items;

  ImplDecl(ImplType t, AstType target,
           std::vector<std::shared_ptr<BaseItem>> items,
           std::optional<std::string> trait = std::nullopt)
      : impl_type(t), target_type(target), trait_name(std::move(trait)),
        associated_items(std::move(items)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class RootNode : public BaseItem {
public:
  std::vector<std::shared_ptr<BaseItem>> children;

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc