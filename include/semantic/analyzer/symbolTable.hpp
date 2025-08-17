#pragma once

#include "ast/types.hpp"

#include "ast/nodes/base.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rc {

class SemanticContext;

enum class SymbolKind {
  Variable,
  Constant,
  Function,
  Struct,
  Enum,
  Module,
  Param
};

struct FunctionTypeInfo {
  std::optional<std::vector<std::pair<std::string, LiteralType>>> parameters;
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
  explicit SymbolChecker(SymbolTable &symbols);

  void build(const std::shared_ptr<RootNode> &root);

  void visit(BaseNode &node) override;

  void visit(FunctionDecl &) override;
  void visit(ConstantItem &) override;
  void visit(ModuleDecl &) override;
  void visit(StructDecl &) override;
  void visit(EnumDecl &) override;
  void visit(TraitDecl &) override;
  void visit(ImplDecl &) override;
  void visit(BlockExpression &) override;
  void visit(RootNode &) override;

private:
  SymbolTable &symbols;
  std::vector<std::string> module_path{};
};

inline bool Scope::declare(const Symbol &sym) {
  if (table_.count(sym.name)) {
    return false;
  }
  table_[sym.name] = sym;
  return true;
}

inline bool Scope::contains(const std::string &name) const {
  return table_.count(name) > 0;
}

inline std::optional<Symbol> Scope::lookup(const std::string &name) const {
  auto it = table_.find(name);
  if (it != table_.end()) {
    return it->second;
  }
  return std::nullopt;
}

inline SymbolTable::SymbolTable() {
  scopes_.clear();
  scopes_.push_back(Scope());
}

inline void SymbolTable::enterScope() { scopes_.push_back(Scope()); }

inline void SymbolTable::exitScope() { scopes_.pop_back(); }

inline std::size_t SymbolTable::depth() const { return scopes_.size(); }

inline bool SymbolTable::declare(const Symbol &sym) {
  if (scopes_.empty()) {
    return false;
  }
  return scopes_.back().declare(sym);
}

inline std::optional<Symbol>
SymbolTable::lookup(const std::string &name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    if (auto sym = it->lookup(name)) {
      return sym;
    }
  }
  return std::nullopt;
}

inline bool SymbolTable::contains(const std::string &name) const {
  for (const auto &scope : scopes_) {
    if (scope.contains(name)) {
      return true;
    }
  }
  return false;
}

inline SymbolChecker::SymbolChecker(SymbolTable &symbols) : symbols(symbols) {}

inline void SymbolChecker::build(const std::shared_ptr<RootNode> &root) {
  visit(*root);
}

inline void SymbolChecker::visit(BaseNode &node) {
  if (auto *func = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*func);
  } else if (auto *const_item = dynamic_cast<ConstantItem *>(&node)) {
    visit(*const_item);
  } else if (auto *module = dynamic_cast<ModuleDecl *>(&node)) {
    visit(*module);
  } else if (auto *struct_decl = dynamic_cast<StructDecl *>(&node)) {
    visit(*struct_decl);
  } else if (auto *enum_decl = dynamic_cast<EnumDecl *>(&node)) {
    visit(*enum_decl);
  } else if (auto *trait_decl = dynamic_cast<TraitDecl *>(&node)) {
    visit(*trait_decl);
  } else if (auto *impl_decl = dynamic_cast<ImplDecl *>(&node)) {
    visit(*impl_decl);
  } else if (auto *root_node = dynamic_cast<RootNode *>(&node)) {
    visit(*root_node);
  } else if (auto *block_expr = dynamic_cast<BlockExpression *>(&node)) {
    visit(*block_expr);
  } else {
    // Do nothing
  }
}

inline void SymbolChecker::visit(RootNode &node) {
  for (const auto &child : node.children) {
    visit(*child);
  }
}

inline void SymbolChecker::visit(FunctionDecl &node) {
  Symbol sym(node.name, SymbolKind::Function);
  sym.function_sig = FunctionTypeInfo{node.params, node.return_type};
  if (!symbols.declare(sym)) {
    throw SemanticException("Duplicate symbol: " + node.name);
  }

  symbols.enterScope();
  if (node.params) {
    for (const auto &param : node.params.value()) {
      Symbol param_sym(param.first, SymbolKind::Param);
      param_sym.type = param.second;
      if (!symbols.declare(param_sym)) {
        throw SemanticException("Duplicate parameter in function '" +
                                node.name + "': " + param.first);
      }
    }
  }

  if (node.body != std::nullopt) {
    visit(*node.body.value());
  }

  symbols.exitScope();
}

inline void SymbolChecker::visit(ConstantItem &node) {
  Symbol sym(node.name, SymbolKind::Constant);
  sym.type = node.type;
  if (!symbols.declare(sym)) {
    throw SemanticException("Duplicate symbol: " + node.name);
  }
}

inline void SymbolChecker::visit(ModuleDecl &node) {
  Symbol sym(node.name, SymbolKind::Module);
  ModuleTypeInfo mi;
  mi.path = module_path;
  mi.path.push_back(node.name);
  sym.module_info = std::move(mi);
  if (!symbols.declare(sym)) {
    throw SemanticException("Duplicate symbol: " + node.name);
  }

  if (node.items) {
    module_path.push_back(node.name);
    symbols.enterScope();
    for (const auto &child : *node.items) {
      visit(*child);
    }
    symbols.exitScope();
    module_path.pop_back();
  }
}

inline void SymbolChecker::visit(StructDecl &node) {
  Symbol sym(node.name, SymbolKind::Struct);
  StructTypeInfo si;
  if (node.struct_type == StructDecl::StructType::Tuple) {
    si.is_tuple = true;
    si.tuple_fields = node.tuple_fields;
  } else {
    si.is_tuple = false;
    si.fields = node.fields;
  }
  sym.struct_info = std::move(si);
  if (!symbols.declare(sym)) {
    throw SemanticException("Duplicate symbol: " + node.name);
  }
}

inline void SymbolChecker::visit(EnumDecl &node) {
  Symbol sym(node.name, SymbolKind::Enum);
  EnumTypeInfo ei;
  ei.variants.reserve(node.variants.size());
  for (const auto &v : node.variants) {
    EnumVariantInfo vi;
    vi.name = v.name;
    vi.tuple_fields = v.tuple_fields;
    vi.struct_fields = v.struct_fields;
    ei.variants.push_back(std::move(vi));
  }
  sym.enum_info = std::move(ei);
  if (!symbols.declare(sym)) {
    throw SemanticException("Duplicate symbol: " + node.name);
  }
}

inline void SymbolChecker::visit(TraitDecl &node) {
  // TODO: do we need to implement trait? leave here.
  symbols.enterScope();
  for (const auto &item : node.associated_items) {
    visit(*item);
  }
  symbols.exitScope();
}

inline void SymbolChecker::visit(ImplDecl &node) {
  // actually, we do not have impl now.
  symbols.enterScope();
  for (const auto &item : node.associated_items) {
    visit(*item);
  }
  symbols.exitScope();
}

inline void SymbolChecker::visit(BlockExpression &node) {
  symbols.enterScope();
  for (const auto &child : node.statements) {
    visit(*child);
  }
  if (node.final_expr) {
    visit(*node.final_expr.value());
  }
  symbols.exitScope();
}

} // namespace rc
