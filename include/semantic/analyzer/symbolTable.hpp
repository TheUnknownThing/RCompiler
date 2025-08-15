#pragma once

#include "ast/types.hpp"

#include "ast/nodes/base.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/symbolTable.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rc {

class SemanticContext;

enum class SymbolKind { Variable, Constant, Function, Struct, Enum, Module };

struct FunctionTypeInfo {
  std::vector<LiteralType> param_types;
  LiteralType return_type;
};

struct StructTypeInfo {
  bool is_tuple = false;
  std::vector<std::pair<std::string, LiteralType>> fields;
  std::vector<LiteralType> tuple_fields;
};

struct EnumVariantInfo {
  std::string name;
  std::optional<std::vector<LiteralType>> tuple_fields;
  std::optional<std::vector<std::pair<std::string, LiteralType>>> struct_fields;
};

struct EnumTypeInfo {
  std::vector<EnumVariantInfo> variants;
};

struct ModuleTypeInfo {
  std::vector<std::string> path;
};

struct Symbol {
  std::string name;
  SymbolKind kind;

  bool is_mutable = false;

  std::optional<LiteralType> type;
  std::optional<FunctionTypeInfo> function_sig;
  std::optional<StructTypeInfo> struct_info;
  std::optional<EnumTypeInfo> enum_info;
  std::optional<ModuleTypeInfo> module_info;

  Symbol() = default;
  Symbol(std::string n, SymbolKind k) : name(std::move(n)), kind(k) {}
};

class Scope {
public:
  bool declare(const Symbol &sym);
  bool contains(const std::string &name) const;
  std::optional<Symbol> lookup(const std::string &name) const;

private:
  std::unordered_map<std::string, Symbol> table_;
};

class SymbolTable {
public:
  SymbolTable();

  void enterScope();
  void exitScope();
  std::size_t depth() const;

  bool declare(const Symbol &sym);

  std::optional<Symbol> lookup(const std::string &name) const;
  bool contains(const std::string &name) const;

private:
  std::vector<Scope> scopes_;
};

class SymbolChecker : public BaseVisitor {
public:
  explicit SymbolChecker(SemanticContext &ctx);

  void build(const std::shared_ptr<RootNode> &root);

  void visit(BaseNode &node) override;

  void visit(FunctionDecl &) override;
  void visit(ConstantItem &) override;
  void visit(ModuleDecl &) override;
  void visit(StructDecl &) override;
  void visit(EnumDecl &) override;
  void visit(TraitDecl &) override;
  void visit(ImplDecl &) override;
  void visit(RootNode &) override;

private:
  SemanticContext &ctx_;
};

} // namespace rc
