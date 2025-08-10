#pragma once

#include "../../lexer/lexer.hpp"
#include "../types.hpp"
#include "base.hpp"
#include "stmt.hpp"
#include "expr.hpp"

#include <memory>
#include <vector>
#include <optional>

namespace rc {
class FunctionDecl : public BaseNode {
public:
  std::string name;
  std::optional<std::vector<std::pair<std::string, LiteralType>>> params;
  LiteralType return_type;
  std::optional<std::shared_ptr<Expression>> body;  // BlockExpression or semicolon

  FunctionDecl(const std::string &nameTok,
               const std::optional<std::vector<std::pair<std::string, LiteralType>>> &params,
               LiteralType return_type,
               std::optional<std::shared_ptr<Expression>> body = std::nullopt)
      : name(nameTok), params(params), return_type(return_type), body(std::move(body)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ConstantItem : public BaseNode {
public:
  std::string name;
  LiteralType type;
  std::optional<std::shared_ptr<Expression>> value;

  ConstantItem(const std::string &n, LiteralType t, 
               std::optional<std::shared_ptr<Expression>> val = std::nullopt)
      : name(n), type(t), value(std::move(val)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ModuleDecl : public BaseNode {
public:
  std::string name;
  std::optional<std::vector<std::unique_ptr<BaseNode>>> items;  // None means semicolon form

  ModuleDecl(const std::string &n, 
             std::optional<std::vector<std::unique_ptr<BaseNode>>> items = std::nullopt)
      : name(n), items(std::move(items)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class StructDecl : public BaseNode {
public:
  enum class StructType { Struct, Tuple };
  
  std::string name;
  StructType struct_type;
  std::vector<std::pair<std::string, LiteralType>> fields;  // For regular struct
  std::vector<LiteralType> tuple_fields;  // For tuple struct

  StructDecl(const std::string &n, StructType t, 
             std::vector<std::pair<std::string, LiteralType>> f = {},
             std::vector<LiteralType> tf = {})
      : name(n), struct_type(t), fields(std::move(f)), tuple_fields(std::move(tf)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class EnumDecl : public BaseNode {
public:
  struct EnumVariant {
    std::string name;
    std::optional<std::vector<LiteralType>> tuple_fields;
    std::optional<std::vector<std::pair<std::string, LiteralType>>> struct_fields;
    std::optional<std::shared_ptr<Expression>> discriminant;
  };

  std::string name;
  std::vector<EnumVariant> variants;

  EnumDecl(const std::string &n, std::vector<EnumVariant> vars)
      : name(n), variants(std::move(vars)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class TraitDecl : public BaseNode {
public:
  std::string name;
  std::vector<std::unique_ptr<BaseNode>> associated_items;

  TraitDecl(const std::string &n, std::vector<std::unique_ptr<BaseNode>> items)
      : name(n), associated_items(std::move(items)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ImplDecl : public BaseNode {
public:
  enum class ImplType { Inherent, Trait };
  
  ImplType impl_type;
  LiteralType target_type;
  std::optional<std::string> trait_name;  // For trait impl
  std::vector<std::unique_ptr<BaseNode>> associated_items;

  ImplDecl(ImplType t, LiteralType target, std::vector<std::unique_ptr<BaseNode>> items,
           std::optional<std::string> trait = std::nullopt)
      : impl_type(t), target_type(target), trait_name(std::move(trait)), 
        associated_items(std::move(items)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class RootNode : public BaseNode {
public:
  std::vector<std::unique_ptr<BaseNode>> children;

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc