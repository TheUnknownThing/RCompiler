#pragma once

/**
 * @details This class does 2 things:
 * 1. Manages scopes for semantic analysis.
 * 2. Stores symbols and their metadata.
 * AND IT CHECKS:
 * 1. Variable usage before declaration.
 * 2. Types (whether they are valid).
 * 3. Duplicated enum fields and struct members.
 * It DOES NOT CHECK:
 * 1. Type consistency (e.g. return type, expr members)
 * 2. Literal Out of Bounds.
 * 3. Invalid member access of Enums and Structs.
 */

#include "ast/types.hpp"

#include "ast/nodes/base.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rc {

class SemanticContext;

enum class ScopeKind {
  Root,
  Module,
  Function,
  Block,
  Trait,
  Impl,
  MatchArm,
  Unknown,
};

enum class SymbolKind {
  Variable,
  Constant,
  Function,
  Struct,
  Enum,
  EnumVariant,
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
  bool isNullary() const { return true; }
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
  std::optional<std::pair<std::string, EnumVariantInfo>> enum_variant_info;

  Symbol() = default;
  Symbol(std::string n, SymbolKind k) : name(std::move(n)), kind(k) {}
};

class Scope {
public:
  bool declare(const Symbol &sym);
  bool declareOrShadow(const Symbol &sym);

  bool contains(const std::string &name) const;
  std::optional<Symbol> lookup(const std::string &name) const;

private:
  std::unordered_map<std::string, Symbol> table_;
};

struct ScopeNode {
  std::size_t id;
  std::size_t depth;
  ScopeKind kind = ScopeKind::Unknown;
  const BaseNode *owner = nullptr;
  std::string label;

  ScopeNode *parent = nullptr;
  std::vector<std::unique_ptr<ScopeNode>> children{};

  std::unordered_map<std::string, Symbol> table;

  std::optional<Symbol> lookupLocal(const std::string &name) const {
    auto it = table.find(name);
    if (it != table.end())
      return it->second;
    return std::nullopt;
  }
};

class SymbolTable {
public:
  SymbolTable();

  void enterScope(ScopeKind kind = ScopeKind::Unknown,
                  const BaseNode *owner = nullptr, std::string label = {});
  void exitScope();
  std::size_t depth() const;

  bool declare(const Symbol &sym);
  bool declareOrShadow(const Symbol &sym);

  std::optional<Symbol> lookup(const std::string &name) const;
  bool contains(const std::string &name) const;

  std::optional<Symbol> lookupInCurrentScope(const std::string &name) const;
  bool containsInCurrentScope(const std::string &name) const;

  const ScopeNode *scopeFor(const BaseNode *owner) const;
  void attachCurrentTo(const BaseNode *owner);

  std::unique_ptr<ScopeNode> root;
  ScopeNode *current = nullptr;
  std::size_t next_id = 0;
  std::unordered_map<const BaseNode *, ScopeNode *> owner_to_scope;
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
  enum class Pass { Declaration, Analysis };
  Pass current_pass_;

  SymbolTable &symbols;
  std::vector<std::string> module_path;
  std::optional<LiteralType> current_let_type;

  static void ensureUniqueStructFields(
      const std::vector<std::pair<std::string, LiteralType>> &fields,
      const std::string &ctx_name);

  static void
  ensureUniqueEnumVariants(const std::vector<EnumVariantInfo> &variants,
                           const std::string &enum_name);

  void validateType(const LiteralType &t) const;
  void validateTypeList(const std::vector<LiteralType> &types) const;
  std::optional<Symbol>
  resolveQualifiedSymbol(const std::vector<std::string> &segments) const;
  static const ScopeNode *findChildModule(const ScopeNode *parent,
                                          const std::string &name);
};

inline bool Scope::declare(const Symbol &sym) {
  if (table_.count(sym.name)) {
    return false;
  }
  table_[sym.name] = sym;
  return true;
}

inline bool Scope::declareOrShadow(const Symbol &sym) {
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
  root = std::make_unique<ScopeNode>();
  root->id = next_id++;
  root->depth = 0;
  root->kind = ScopeKind::Root;
  root->label = "<root>";
  root->parent = nullptr;
  current = root.get();
}

inline void SymbolTable::enterScope(ScopeKind kind, const BaseNode *owner,
                                    std::string label) {
  auto node = std::make_unique<ScopeNode>();
  node->id = next_id++;
  node->depth = current ? current->depth + 1 : 0;
  node->kind = kind;
  node->owner = owner;
  node->label = std::move(label);
  node->parent = current;

  ScopeNode *raw = node.get();
  if (current) {
    current->children.push_back(std::move(node));
  } else {
    root = std::move(node);
  }
  current = raw;
  if (owner)
    owner_to_scope[owner] = raw;
}

inline void SymbolTable::exitScope() {
  if (!current)
    return;
  current = current->parent ? current->parent : current;
}

inline std::size_t SymbolTable::depth() const {
  return current ? (current->depth + 1) : 0;
}

inline bool SymbolTable::declare(const Symbol &sym) {
  if (!current)
    return false;
  if (current->table.count(sym.name))
    return false;
  current->table[sym.name] = sym;
  return true;
}

inline bool SymbolTable::declareOrShadow(const Symbol &sym) {
  if (!current)
    return false;
  current->table[sym.name] = sym;
  return true;
}

inline std::optional<Symbol>
SymbolTable::lookup(const std::string &name) const {
  const ScopeNode *node = current;
  while (node) {
    auto it = node->table.find(name);
    if (it != node->table.end())
      return it->second;
    node = node->parent;
  }
  return std::nullopt;
}

inline bool SymbolTable::contains(const std::string &name) const {
  return lookup(name).has_value();
}

inline std::optional<Symbol>
SymbolTable::lookupInCurrentScope(const std::string &name) const {
  if (!current)
    return std::nullopt;
  auto it = current->table.find(name);
  if (it != current->table.end())
    return it->second;
  return std::nullopt;
}

inline bool SymbolTable::containsInCurrentScope(const std::string &name) const {
  return current && current->table.count(name) > 0;
}

inline const ScopeNode *SymbolTable::scopeFor(const BaseNode *owner) const {
  auto it = owner_to_scope.find(owner);
  if (it != owner_to_scope.end())
    return it->second;
  return nullptr;
}

inline void SymbolTable::attachCurrentTo(const BaseNode *owner) {
  if (!owner || !current)
    return;
  owner_to_scope[owner] = current;
}

inline SymbolChecker::SymbolChecker(SymbolTable &symbols) : symbols(symbols) {}

inline void SymbolChecker::build(const std::shared_ptr<RootNode> &root) {
  current_pass_ = Pass::Declaration;
  visit(*root);

  current_pass_ = Pass::Analysis;
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
  if (current_pass_ == Pass::Declaration) {
    Symbol sym(node.name, SymbolKind::Function);
    sym.function_sig = FunctionTypeInfo{node.params, node.return_type};
    if (!symbols.declare(sym)) {
      throw SemanticException("Duplicate function: " + node.name);
    }
  } else {
    if (node.params) {
      for (const auto &param : node.params.value()) {
        validateType(param.second);
      }
    }
    validateType(node.return_type);

    symbols.enterScope(ScopeKind::Function, &node, node.name);
    if (node.params) {
      for (const auto &param : node.params.value()) {
        if (symbols.containsInCurrentScope(param.first)) {
          throw SemanticException("Duplicate parameter in function '" +
                                  node.name + "': " + param.first);
        }
        Symbol param_sym(param.first, SymbolKind::Param);
        param_sym.type = param.second;
        symbols.declare(param_sym);
      }
    }
    if (node.body) {
      visit(*node.body.value());
    }
    symbols.exitScope();
  }
}

inline void SymbolChecker::visit(ConstantItem &node) {
  if (current_pass_ == Pass::Declaration) {
    Symbol sym(node.name, SymbolKind::Constant);
    sym.type = node.type;
    if (!symbols.declare(sym)) {
      throw SemanticException("Duplicate constant: " + node.name);
    }
  } else {
    validateType(node.type);
    if (node.value) {
      visit(*node.value.value());
    }
  }
}

inline void SymbolChecker::visit(ModuleDecl &node) {
  if (current_pass_ == Pass::Declaration) {
    Symbol sym(node.name, SymbolKind::Module);
    ModuleTypeInfo mi;
    mi.path = module_path;
    mi.path.push_back(node.name);
    sym.module_info = std::move(mi);
    if (!symbols.declare(sym)) {
      throw SemanticException("Duplicate module: " + node.name);
    }
  }

  if (node.items) {
    module_path.push_back(node.name);
    symbols.enterScope(ScopeKind::Module, &node, node.name);
    for (const auto &child : *node.items) {
      visit(*child);
    }
    symbols.exitScope();
    module_path.pop_back();
  }
}

inline void SymbolChecker::visit(StructDecl &node) {
  if (current_pass_ == Pass::Declaration) {
    if (node.struct_type != StructDecl::StructType::Tuple) {
      ensureUniqueStructFields(node.fields, node.name);
    }

    Symbol sym(node.name, SymbolKind::Struct);
    StructTypeInfo si;
    si.is_tuple = (node.struct_type == StructDecl::StructType::Tuple);
    if (si.is_tuple) {
      si.tuple_fields = node.tuple_fields;
    } else {
      si.fields = node.fields;
    }
    sym.struct_info = std::move(si);
    if (!symbols.declare(sym)) {
      throw SemanticException("Duplicate struct: " + node.name);
    }
  } else {
    if (node.struct_type == StructDecl::StructType::Tuple) {
      validateTypeList(node.tuple_fields);
    } else {
      for (const auto &f : node.fields) {
        validateType(f.second);
      }
    }
  }
}

inline void SymbolChecker::visit(EnumDecl &node) {
  if (current_pass_ == Pass::Declaration) {
    EnumTypeInfo ei;
    ei.variants.reserve(node.variants.size());
    for (const auto &v : node.variants) {
      EnumVariantInfo vi;
      vi.name = v.name;
      ei.variants.push_back(std::move(vi));
    }

    ensureUniqueEnumVariants(ei.variants, node.name);

    Symbol enum_sym(node.name, SymbolKind::Enum);
    enum_sym.enum_info = ei;
    if (!symbols.declare(enum_sym)) {
      throw SemanticException("Duplicate enum: " + node.name);
    }

    for (const auto &variant_info : enum_sym.enum_info->variants) {
      Symbol variant_sym(variant_info.name, SymbolKind::EnumVariant);
      variant_sym.enum_variant_info = std::make_pair(node.name, variant_info);
      symbols.declareOrShadow(variant_sym);
    }
  }
}

inline void SymbolChecker::visit(TraitDecl &node) {
  symbols.enterScope(ScopeKind::Trait, &node, node.name);
  for (const auto &item : node.associated_items) {
    visit(*item);
  }
  symbols.exitScope();
}

inline void SymbolChecker::visit(ImplDecl &node) {
  symbols.enterScope(ScopeKind::Impl, &node, "impl");
  for (const auto &item : node.associated_items) {
    visit(*item);
  }
  symbols.exitScope();
}

inline void SymbolChecker::visit(IdentifierPattern &node) {
  if (current_pass_ == Pass::Declaration)
    return;

  if (node.name == "_") {
    if (node.subpattern) {
      node.subpattern.value()->accept(*this);
    }
    return;
  }

  bool is_binding = false;

  if (node.subpattern) {
    is_binding = true;
  } else {
    auto existing_sym = symbols.lookup(node.name);
    if (existing_sym &&
        (existing_sym->kind == SymbolKind::Constant ||
         (existing_sym->kind == SymbolKind::EnumVariant &&
          existing_sym->enum_variant_info->second.isNullary()))) {
      is_binding = false;
    } else {
      is_binding = true;
    }
  }

  if (is_binding) {
    Symbol sym(node.name, SymbolKind::Variable);
    sym.is_mutable = node.is_mutable;
    if (current_let_type) {
      sym.type = *current_let_type;
    }
    symbols.declareOrShadow(sym);
  }

  if (node.subpattern) {
    node.subpattern.value()->accept(*this);
  }
}

inline void SymbolChecker::visit(BlockExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  symbols.enterScope(ScopeKind::Block, &node, "block");
  for (const auto &child : node.statements) {
    visit(*child);
  }
  if (node.final_expr) {
    visit(*node.final_expr.value());
  }
  symbols.exitScope();
}

inline void SymbolChecker::visit(NameExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (!symbols.lookup(node.name)) {
    throw UndefinedVariableError(node.name);
  }
}

inline void SymbolChecker::visit(LiteralExpression &node) { (void)node; }

inline void SymbolChecker::visit(PrefixExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.right) {
    node.right->accept(*this);
  }
}

inline void SymbolChecker::visit(BinaryExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.left)
    node.left->accept(*this);
  if (node.right)
    node.right->accept(*this);
}

inline void SymbolChecker::visit(GroupExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.inner)
    node.inner->accept(*this);
}

inline void SymbolChecker::visit(IfExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.condition)
    node.condition->accept(*this);
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

inline void SymbolChecker::visit(CallExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.function_name)
    node.function_name->accept(*this);
  for (const auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

inline void SymbolChecker::visit(MethodCallExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.receiver)
    node.receiver->accept(*this);
  for (const auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

inline void SymbolChecker::visit(FieldAccessExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.target)
    node.target->accept(*this);
}

inline void SymbolChecker::visit(MatchExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.scrutinee)
    node.scrutinee->accept(*this);
  for (const auto &arm : node.arms) {
    symbols.enterScope(ScopeKind::MatchArm, nullptr, "match_arm");
    if (arm.pattern)
      arm.pattern->accept(*this);
    if (arm.guard)
      arm.guard.value()->accept(*this);
    if (arm.body)
      arm.body->accept(*this);
    symbols.exitScope();
  }
}

inline void SymbolChecker::visit(ReturnExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.value)
    node.value.value()->accept(*this);
}

inline void SymbolChecker::visit(UnderscoreExpression &node) { (void)node; }

inline void SymbolChecker::visit(LoopExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.body)
    node.body->accept(*this);
}

inline void SymbolChecker::visit(WhileExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.condition)
    node.condition->accept(*this);
  if (node.body)
    node.body->accept(*this);
}

inline void SymbolChecker::visit(ArrayExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
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
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.target)
    node.target->accept(*this);
  if (node.index)
    node.index->accept(*this);
}

inline void SymbolChecker::visit(TupleExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  for (const auto &el : node.elements) {
    if (el)
      el->accept(*this);
  }
}

inline void SymbolChecker::visit(BreakExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.expr)
    node.expr.value()->accept(*this);
}

inline void SymbolChecker::visit(ContinueExpression &node) { (void)node; }

inline void SymbolChecker::visit(PathExpression &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  // TODO: resolve path
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
  if (current_pass_ == Pass::Declaration)
    return;
  PathExpression tmp(false, {});
  tmp.segments = node.segments;
  visit(tmp);
}

inline void SymbolChecker::visit(BlockStatement &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  symbols.enterScope(ScopeKind::Block, &node, "block");
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
  symbols.exitScope();
}

inline void SymbolChecker::visit(LetStatement &node) {
  if (current_pass_ == Pass::Declaration)
    return;

  validateType(node.type);
  if (node.expr)
    node.expr->accept(*this);

  current_let_type = node.type;
  if (node.pattern) {
    node.pattern->accept(*this);
  }
  current_let_type.reset();
}

inline void SymbolChecker::visit(ExpressionStatement &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.expression)
    node.expression->accept(*this);
}

inline void SymbolChecker::visit(EmptyStatement &node) { (void)node; }

inline void SymbolChecker::visit(LiteralPattern &node) { (void)node; }

inline void SymbolChecker::visit(WildcardPattern &node) { (void)node; }

inline void SymbolChecker::visit(ReferencePattern &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  if (node.inner_pattern)
    node.inner_pattern->accept(*this);
}

inline void SymbolChecker::visit(StructPattern &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  for (const auto &field : node.fields) {
    if (field.pattern)
      field.pattern->accept(*this);
  }
}

inline void SymbolChecker::visit(PathPattern &node) { (void)node; }

inline void SymbolChecker::visit(OrPattern &node) {
  if (current_pass_ == Pass::Declaration)
    return;
  for (auto &alt : node.alternatives)
    if (alt)
      alt->accept(*this);
}

inline void SymbolChecker::ensureUniqueStructFields(
    const std::vector<std::pair<std::string, LiteralType>> &fields,
    const std::string &ctx_name) {
  std::unordered_set<std::string> seen;
  for (const auto &f : fields) {
    if (!seen.insert(f.first).second) {
      throw SemanticException("Duplicate field '" + f.first + "' in struct '" +
                              ctx_name + "'");
    }
  }
}

inline void SymbolChecker::ensureUniqueEnumVariants(
    const std::vector<EnumVariantInfo> &variants,
    const std::string &enum_name) {
  std::unordered_set<std::string> seen;
  for (const auto &v : variants) {
    if (!seen.insert(v.name).second) {
      throw SemanticException("Duplicate variant '" + v.name + "' in enum '" +
                              enum_name + "'");
    }
  }
}

inline void SymbolChecker::validateType(const LiteralType &t) const {
  if (t.is_base()) {
    return; // valid
  }
  if (t.is_tuple()) {
    validateTypeList(t.as_tuple());
    return;
  }
  if (t.is_array()) {
    const auto &arr = t.as_array();
    if (!arr.element) {
      throw TypeError("Invalid array element type");
    }
    validateType(*arr.element);
    return;
  }
  if (t.is_slice()) {
    const auto &sl = t.as_slice();
    if (!sl.element) {
      throw TypeError("Invalid slice element type");
    }
    validateType(*sl.element);
    return;
  }
  if (t.is_union()) {
    validateTypeList(t.as_union());
    return;
  }
  if (t.is_path()) {
    const auto &segs = t.as_path().segments;
    if (segs.empty()) {
      throw TypeError("Empty type path");
    }
    auto resolved = resolveQualifiedSymbol(segs);
    if (!resolved) {
      throw TypeError("Unknown type when parsing Path.");
    }
    switch (resolved->kind) {
    case SymbolKind::Struct:
    case SymbolKind::Enum:
      return; // resolved, ok
    default:
      throw TypeError("Not a type when parsing Path.");
    }
  }
}

inline void
SymbolChecker::validateTypeList(const std::vector<LiteralType> &types) const {
  for (const auto &t : types) {
    validateType(t);
  }
}

inline const ScopeNode *
SymbolChecker::findChildModule(const ScopeNode *parent,
                               const std::string &name) {
  if (!parent)
    return nullptr;
  for (const auto &child : parent->children) {
    if (child && child->kind == ScopeKind::Module && child->label == name) {
      return child.get();
    }
  }
  return nullptr;
}

inline std::optional<Symbol> SymbolChecker::resolveQualifiedSymbol(
    const std::vector<std::string> &segments) const {
  if (segments.empty())
    return std::nullopt;

  const auto &last = segments.back();

  if (segments.size() == 1) {
    const ScopeNode *node = symbols.current;
    while (node) {
      auto local = node->lookupLocal(last);
      if (local) {
        if (local->kind == SymbolKind::Struct ||
            local->kind == SymbolKind::Enum)
          return local;
      }
      node = node->parent;
    }
    return std::nullopt;
  }

  // qualified path
  const ScopeNode *anchor = symbols.current;
  while (anchor) {
    const ScopeNode *cursor = anchor;
    bool ok = true;
    for (size_t i = 0; i + 1 < segments.size(); ++i) {
      cursor = findChildModule(cursor, segments[i]);
      if (!cursor) {
        ok = false;
        break;
      }
    }
    if (ok && cursor) {
      auto local = cursor->lookupLocal(last);
      if (local)
        return local;
    }
    anchor = anchor->parent;
  }

  // absolute from root
  const ScopeNode *cursor = symbols.root.get();
  bool ok = true;
  for (size_t i = 0; i + 1 < segments.size(); ++i) {
    cursor = findChildModule(cursor, segments[i]);
    if (!cursor) {
      ok = false;
      break;
    }
  }
  if (ok && cursor) {
    auto local = cursor->lookupLocal(last);
    if (local)
      return local;
  }
  return std::nullopt;
}

} // namespace rc