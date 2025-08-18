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
  bool overwrite(const Symbol &sym);
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
  bool overwrite(const Symbol &sym);

  std::optional<Symbol> lookup(const std::string &name) const;
  bool contains(const std::string &name) const;

  std::optional<Symbol> lookupInCurrentScope(const std::string &name) const;
  bool containsInCurrentScope(const std::string &name) const;

private:
  std::vector<Scope> scopes_;
};

class SymbolChecker : public BaseVisitor {
public:
  explicit SymbolChecker(SymbolTable &symbols);

  void build(const std::shared_ptr<RootNode> &root);

  void visit(BaseNode &node) override;

  // Expression visitors
  void visit(NameExpression &node) override;
  void visit(LiteralExpression &node) override;
  void visit(PrefixExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(GroupExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(MatchExpression &node) override;
  void visit(ReturnExpression &node) override;
  void visit(CallExpression &node) override;
  void visit(MethodCallExpression &node) override;
  void visit(FieldAccessExpression &node) override;
  void visit(UnderscoreExpression &node) override;
  void visit(BlockExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(ArrayExpression &node) override;
  void visit(IndexExpression &node) override;
  void visit(TupleExpression &node) override;
  void visit(BreakExpression &node) override;
  void visit(ContinueExpression &node) override;
  void visit(PathExpression &node) override;
  void visit(QualifiedPathExpression &node) override;

  // Statement visitors
  void visit(BlockStatement &node) override;
  void visit(LetStatement &node) override;
  void visit(ExpressionStatement &node) override;
  void visit(EmptyStatement &node) override;

  // Pattern visitors
  void visit(IdentifierPattern &node) override;
  void visit(LiteralPattern &node) override;
  void visit(WildcardPattern &node) override;
  void visit(ReferencePattern &node) override;
  void visit(StructPattern &node) override;
  void visit(PathPattern &node) override;
  void visit(OrPattern &node) override;

  // Top-level declaration visitors
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(ModuleDecl &node) override;
  void visit(StructDecl &node) override;
  void visit(EnumDecl &node) override;
  void visit(TraitDecl &node) override;
  void visit(ImplDecl &node) override;
  void visit(RootNode &node) override;

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

inline bool Scope::overwrite(const Symbol &sym) {
  if (table_.count(sym.name) == 0) {
    return false;
  }
  table_[sym.name] = sym;
  return true;
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

inline bool SymbolTable::overwrite(const Symbol &sym) {
  if (scopes_.empty()) {
    return false;
  }
  return scopes_.back().overwrite(sym);
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

inline std::optional<Symbol>
SymbolTable::lookupInCurrentScope(const std::string &name) const {
  if (!scopes_.empty()) {
    return scopes_.back().lookup(name);
  }
  return std::nullopt;
}

inline bool SymbolTable::containsInCurrentScope(const std::string &name) const {
  if (!scopes_.empty()) {
    return scopes_.back().contains(name);
  }
  return false;
}

inline SymbolChecker::SymbolChecker(SymbolTable &symbols) : symbols(symbols) {}

inline void SymbolChecker::build(const std::shared_ptr<RootNode> &root) {
  visit(*root);
}

inline void SymbolChecker::visit(BaseNode &node) {
  if (auto *expr = dynamic_cast<NameExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LiteralExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<PrefixExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BinaryExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<GroupExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<MatchExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ReturnExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<CallExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<MethodCallExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<FieldAccessExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<UnderscoreExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ArrayExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IndexExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<TupleExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BreakExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ContinueExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<PathExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<QualifiedPathExpression *>(&node)) {
    visit(*expr);
  } else if (auto *stmt = dynamic_cast<BlockStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<EmptyStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ConstantItem *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ModuleDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<StructDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<EnumDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<TraitDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ImplDecl *>(&node)) {
    visit(*decl);
  } else if (auto *p = dynamic_cast<IdentifierPattern *>(&node)) {
    visit(*p);
  } else if (auto *p2 = dynamic_cast<LiteralPattern *>(&node)) {
    visit(*p2);
  } else if (auto *p3 = dynamic_cast<WildcardPattern *>(&node)) {
    visit(*p3);
  } else if (auto *p5 = dynamic_cast<ReferencePattern *>(&node)) {
    visit(*p5);
  } else if (auto *p6 = dynamic_cast<StructPattern *>(&node)) {
    visit(*p6);
  } else if (auto *p9 = dynamic_cast<PathPattern *>(&node)) {
    visit(*p9);
  } else if (auto *p11 = dynamic_cast<OrPattern *>(&node)) {
    visit(*p11);
  } else if (auto *root = dynamic_cast<RootNode *>(&node)) {
    visit(*root);
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
  if (node.value) {
    visit(*node.value.value());
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

inline void SymbolChecker::visit(NameExpression &node) {
  if (!symbols.lookup(node.name).has_value()) {
    throw UndefinedVariableError(node.name);
  }
}

inline void SymbolChecker::visit(LiteralExpression &node) { (void)node; }

inline void SymbolChecker::visit(PrefixExpression &node) {
  if (node.right) {
    node.right->accept(*this);
  }
}

inline void SymbolChecker::visit(BinaryExpression &node) {
  if (node.left)
    node.left->accept(*this);
  if (node.right)
    node.right->accept(*this);
}

inline void SymbolChecker::visit(GroupExpression &node) {
  if (node.inner)
    node.inner->accept(*this);
}

inline void SymbolChecker::visit(IfExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

inline void SymbolChecker::visit(CallExpression &node) {
  if (node.function_name)
    node.function_name->accept(*this);
  for (const auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

inline void SymbolChecker::visit(MethodCallExpression &node) {
  if (node.receiver)
    node.receiver->accept(*this);
  for (const auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

inline void SymbolChecker::visit(FieldAccessExpression &node) {
  if (node.target)
    node.target->accept(*this);
}

inline void SymbolChecker::visit(MatchExpression &node) {
  if (node.scrutinee)
    node.scrutinee->accept(*this);
  for (const auto &arm : node.arms) {
    if (arm.guard)
      arm.guard.value()->accept(*this);
    if (arm.body)
      arm.body->accept(*this);
  }
}

inline void SymbolChecker::visit(ReturnExpression &node) {
  if (node.value)
    node.value.value()->accept(*this);
}

inline void SymbolChecker::visit(UnderscoreExpression &node) { (void)node; }

inline void SymbolChecker::visit(LoopExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

inline void SymbolChecker::visit(WhileExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.body)
    node.body->accept(*this);
}

inline void SymbolChecker::visit(ArrayExpression &node) {
  if (node.repeat) {
    if (node.repeat->first)
      node.repeat->first->accept(*this);
    if (node.repeat->second)
      node.repeat->second->accept(*this);
  } else {
    for (const auto &el : node.elements) {
      if (el)
        el->accept(*this);
    }
  }
}

inline void SymbolChecker::visit(IndexExpression &node) {
  if (node.target)
    node.target->accept(*this);
  if (node.index)
    node.index->accept(*this);
}

inline void SymbolChecker::visit(TupleExpression &node) {
  for (const auto &el : node.elements) {
    if (el)
      el->accept(*this);
  }
}

inline void SymbolChecker::visit(BreakExpression &node) {
  if (node.expr)
    node.expr.value()->accept(*this);
}

inline void SymbolChecker::visit(ContinueExpression &node) { (void)node; }

inline void SymbolChecker::visit(PathExpression &node) {
  for (const auto &seg : node.segments) {
    if (seg.call) {
      for (const auto &arg : seg.call->args) {
        if (arg)
          arg->accept(*this);
      }
    }
  }
}

inline void SymbolChecker::visit(QualifiedPathExpression &node) {
  PathExpression tmp(false, {});
  tmp.segments = node.segments;
  visit(tmp);
}

inline void SymbolChecker::visit(BlockStatement &node) {
  symbols.enterScope();
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
  symbols.exitScope();
}

inline void SymbolChecker::visit(LetStatement &node) {
  if (node.expr)
    node.expr->accept(*this);

  if (node.pattern) {
    node.pattern->accept(*this);
  }
}

inline void SymbolChecker::visit(ExpressionStatement &node) {
  if (node.expression)
    node.expression->accept(*this);
}

inline void SymbolChecker::visit(EmptyStatement &node) { (void)node; }

inline void SymbolChecker::visit(IdentifierPattern &node) {
  if (node.name != "_") {
    Symbol sym(node.name, SymbolKind::Variable);
    sym.is_mutable = node.is_mutable;
  }

  if (node.subpattern) {
    node.subpattern.value()->accept(*this);
  }
}

inline void SymbolChecker::visit(LiteralPattern &node) { (void)node; }

inline void SymbolChecker::visit(WildcardPattern &node) { (void)node; }

inline void SymbolChecker::visit(ReferencePattern &node) {
  if (node.inner_pattern)
    node.inner_pattern->accept(*this);
}

inline void SymbolChecker::visit(StructPattern &node) {
  for (const auto &field : node.fields) {
    if (field.pattern)
      field.pattern->accept(*this);
  }
}

inline void SymbolChecker::visit(PathPattern &node) { (void)node; }

inline void SymbolChecker::visit(OrPattern &node) {
  for (auto &alt : node.alternatives)
    if (alt)
      alt->accept(*this);
}

} // namespace rc
