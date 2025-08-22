#pragma once

/**
 * @details This class CHECKS:
 * 1. Operator compatibility in Expressions
 * 2. Type compatibility
 */

#include "ast/nodes/expr.hpp"
#include "ast/nodes/stmt.hpp"
#include "semantic/analyzer/symbolTable.hpp"
#include "semantic/error/exceptions.hpp"

#include <memory>
#include <optional>

namespace rc {

class SemanticContext;

class ExprAnalyzer : public BaseVisitor {
public:
  explicit ExprAnalyzer(SymbolTable &sym);

  std::optional<LiteralType> analyze(const std::shared_ptr<Expression> &expr);

  void visit(BaseNode &node) override;

  // Expression visitors
  void visit(NameExpression &) override;
  void visit(LiteralExpression &) override;
  void visit(PrefixExpression &) override;
  void visit(BinaryExpression &) override;
  void visit(GroupExpression &) override;
  void visit(IfExpression &) override;
  void visit(MatchExpression &) override;
  void visit(ReturnExpression &) override;
  void visit(CallExpression &) override;
  void visit(MethodCallExpression &) override;
  void visit(FieldAccessExpression &) override;
  void visit(StructExpression &) override;
  void visit(UnderscoreExpression &) override;
  void visit(BlockExpression &) override;
  void visit(LoopExpression &) override;
  void visit(WhileExpression &) override;
  void visit(ArrayExpression &) override;
  void visit(IndexExpression &) override;
  void visit(TupleExpression &) override;
  void visit(BreakExpression &) override;
  void visit(ContinueExpression &) override;
  void visit(PathExpression &) override;
  void visit(QualifiedPathExpression &) override;

  // Statement visitors
  void visit(BlockStatement &) override;
  void visit(LetStatement &) override;
  void visit(ExpressionStatement &) override;
  void visit(EmptyStatement &) override;

  // Pattern visitors
  void visit(IdentifierPattern &) override;
  void visit(LiteralPattern &) override;
  void visit(WildcardPattern &) override;
  void visit(RestPattern &) override;
  void visit(ReferencePattern &) override;
  void visit(StructPattern &) override;
  void visit(TuplePattern &) override;
  void visit(GroupedPattern &) override;
  void visit(PathPattern &) override;
  void visit(SlicePattern &) override;
  void visit(OrPattern &) override;

  // Top-level declaration visitors
  void visit(FunctionDecl &) override;
  void visit(ConstantItem &) override;
  void visit(ModuleDecl &) override;
  void visit(StructDecl &) override;
  void visit(EnumDecl &) override;
  void visit(TraitDecl &) override;
  void visit(ImplDecl &) override;
  void visit(RootNode &) override;

  std::optional<LiteralType> result() const;

private:
  SymbolTable &symbol_table;

  std::optional<LiteralType> last_type;

  std::optional<LiteralType> evaluateBinary(const LiteralType &lhs,
                                            const rc::Token &op,
                                            const LiteralType &rhs);
  std::optional<LiteralType> literalToType(const LiteralExpression &lit);

  void enterScope(ScopeKind kind = ScopeKind::Block,
                  const BaseNode *owner = nullptr,
                  const std::string &label = {});
  void exitScope();

  std::optional<LiteralType> resolveNameType(const std::string &name);
  std::optional<LiteralType>
  resolveFunctionCallType(const std::string &name,
                          const std::vector<LiteralType> &arg_types);
};

inline ExprAnalyzer::ExprAnalyzer(SymbolTable &sym)
    : symbol_table(sym), last_type(std::nullopt) {}

static bool is_integer(const rc::LiteralType &t) {
  return t.is_base() && (t.as_base() == rc::PrimitiveLiteralType::I32 ||
                         t.as_base() == rc::PrimitiveLiteralType::U32 ||
                         t.as_base() == rc::PrimitiveLiteralType::ISIZE ||
                         t.as_base() == rc::PrimitiveLiteralType::USIZE);
}

static bool is_bool(const rc::LiteralType &t) {
  return t.is_base() && t.as_base() == rc::PrimitiveLiteralType::BOOL;
}

static bool is_string(const rc::LiteralType &t) {
  return t.is_base() && (t.as_base() == rc::PrimitiveLiteralType::STRING ||
                         t.as_base() == rc::PrimitiveLiteralType::C_STRING ||
                         t.as_base() == rc::PrimitiveLiteralType::RAW_STRING ||
                         t.as_base() == rc::PrimitiveLiteralType::RAW_C_STRING);
}

inline std::optional<LiteralType>
ExprAnalyzer::analyze(const std::shared_ptr<Expression> &expr) {
  last_type.reset();
  if (!expr)
    return std::nullopt;
  expr->accept(*this);
  return last_type;
}

inline void ExprAnalyzer::visit(BaseNode &node) {
  // Expressions
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

    // Statements
  } else if (auto *stmt = dynamic_cast<BlockStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<EmptyStatement *>(&node)) {
    visit(*stmt);

    // Top-level
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

    // Patterns
  } else if (auto *p = dynamic_cast<IdentifierPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<LiteralPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<WildcardPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<RestPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<ReferencePattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<StructPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<TuplePattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<GroupedPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<PathPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<SlicePattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<OrPattern *>(&node)) {
    visit(*p);

  } else if (auto *root_node = dynamic_cast<RootNode *>(&node)) {
    visit(*root_node);
  } else {
    // No-op
  }
}

inline std::optional<LiteralType>
ExprAnalyzer::evaluateBinary(const LiteralType &lhs, const rc::Token &op,
                             const LiteralType &rhs) {
  switch (op.type) {
  case rc::TokenType::PLUS:
    if (is_string(lhs) && is_string(rhs) && lhs == rhs) {
      return lhs;
    }
  case rc::TokenType::MINUS:
  case rc::TokenType::STAR:
  case rc::TokenType::SLASH:
  case rc::TokenType::PERCENT: {
    if (is_integer(lhs) && is_integer(rhs) && lhs == rhs) {
      return lhs;
    }
    return std::nullopt;
  }

  // bitwise
  case rc::TokenType::AMPERSAND:
  case rc::TokenType::PIPE:
  case rc::TokenType::CARET: {
    if (is_integer(lhs) && is_integer(rhs) && lhs == rhs) {
      return lhs;
    }
    return std::nullopt;
  }

  // << >>
  case rc::TokenType::SHL:
  case rc::TokenType::SHR: {
    if (is_integer(lhs) && is_integer(rhs)) {
      return lhs;
    }
    return std::nullopt;
  }

  // bool
  case rc::TokenType::AND:
  case rc::TokenType::OR: {
    if (is_bool(lhs) && is_bool(rhs)) {
      return LiteralType::base(PrimitiveLiteralType::BOOL);
    }
    return std::nullopt;
  }

  case rc::TokenType::EQ:
  case rc::TokenType::NE: {
    if (lhs.is_base() && rhs.is_base() && lhs == rhs) {
      return LiteralType::base(PrimitiveLiteralType::BOOL);
    }
    return std::nullopt;
  }

  case rc::TokenType::LT:
  case rc::TokenType::LE:
  case rc::TokenType::GT:
  case rc::TokenType::GE: {
    if (is_integer(lhs) && is_integer(rhs) && lhs == rhs) {
      return LiteralType::base(PrimitiveLiteralType::BOOL);
    }
    return std::nullopt;
  }

  default:
    return std::nullopt;
  }
}

inline void ExprAnalyzer::visit(RootNode &node) {
  for (const auto &child : node.children) {
    if (child)
      child->accept(*this);
  }
}

// Expressions
inline void ExprAnalyzer::visit(NameExpression &node) {
  auto type = resolveNameType(node.name);
  if (!type) {
    throw UndefinedVariableError(node.name);
  }
  last_type = type;
}

inline void ExprAnalyzer::visit(LiteralExpression &node) {
  last_type = literalToType(node);
}

inline void ExprAnalyzer::visit(PrefixExpression &node) {
  auto rt = analyze(node.right);
  if (!rt) {
    last_type.reset();
    return;
  }
  switch (node.op.type) {
  case rc::TokenType::NOT: {
    if (!is_bool(*rt))
      throw TypeError("Operator '!' requires bool operand");
    last_type = LiteralType::base(PrimitiveLiteralType::BOOL);
    break;
  }
  case rc::TokenType::MINUS: {
    if (!is_integer(*rt))
      throw TypeError("Unary '-' requires integer operand");
    last_type = *rt;
    break;
  }
  default:
    // Unsupported unary operator
    last_type.reset();
    break;
  }
}

inline void ExprAnalyzer::visit(BinaryExpression &node) {
  auto lt = analyze(node.left);
  auto rt = analyze(node.right);
  if (!lt || !rt) {
    last_type.reset();
    return;
  }
  auto res = evaluateBinary(*lt, node.op, *rt);
  if (!res) {
    throw TypeError("Operator '" + std::string(rc::toString(node.op.type)) +
                    "' not applicable to types '" + to_string(*lt) + "' and '" +
                    to_string(*rt) + "'");
  }
  last_type = res;
}

inline void ExprAnalyzer::visit(GroupExpression &node) {
  last_type = analyze(node.inner);
}

inline void ExprAnalyzer::visit(IfExpression &node) {
  auto cond = analyze(node.condition);
  if (!cond || !is_bool(*cond)) {
    throw TypeError("If condition must be boolean");
  }
  last_type = analyze(node.then_block);
  if (node.else_block) {
    last_type = analyze(node.else_block.value());
  }
}

inline void ExprAnalyzer::visit(MatchExpression &node) {
  // No match expr anymore, yay
  (void)node;
}

inline void ExprAnalyzer::visit(ReturnExpression &node) {
  if (node.value) {
    last_type = analyze(node.value.value());
    if (!last_type) {
      last_type = LiteralType::base(PrimitiveLiteralType::NONE);
    }
  } else {
    last_type = LiteralType::base(PrimitiveLiteralType::NONE);
  }
}

inline void ExprAnalyzer::visit(CallExpression &node) {
  auto func_type = analyze(node.function_name);
  if (!func_type) {
    last_type.reset();
    return;
  }

  std::vector<LiteralType> arg_types;
  for (const auto &arg : node.arguments) {
    auto arg_type = analyze(arg);
    if (!arg_type) {
      last_type.reset();
      return;
    }
    arg_types.push_back(*arg_type);
  }

  // TODO: We need to set last_type to function reture type.
}

inline void ExprAnalyzer::visit(MethodCallExpression &node) {
  analyze(node.receiver);
  for (auto &arg : node.arguments) {
    analyze(arg);
  }
  last_type.reset();
}

inline void ExprAnalyzer::visit(FieldAccessExpression &node) {
  analyze(node.target);
  last_type.reset();
}

inline void ExprAnalyzer::visit(StructExpression &node) {
  // Struct expr now only contains identifier, so do not need checking
  (void)node;
}

inline void ExprAnalyzer::visit(UnderscoreExpression &) {
  last_type = LiteralType::base(PrimitiveLiteralType::NONE);
}

inline void ExprAnalyzer::visit(BlockExpression &node) {
  enterScope(ScopeKind::Block, &node, "block");
  for (const auto &stmt : node.statements) {
    if (stmt)
      stmt->accept(*this);
  }
  if (node.final_expr) {
    last_type = analyze(node.final_expr.value());
  } else {
    last_type = LiteralType::base(PrimitiveLiteralType::NONE);
  }
  exitScope();
}

inline void ExprAnalyzer::visit(LoopExpression &node) {
  last_type = analyze(node.body);
}

inline void ExprAnalyzer::visit(WhileExpression &node) {
  auto cond = analyze(node.condition);
  if (!cond || !is_bool(*cond)) {
    throw TypeError("While condition must be boolean");
  }
  last_type = analyze(node.body);
}

inline void ExprAnalyzer::visit(ArrayExpression &node) {
  if (node.repeat) {
    auto elem_t = analyze(node.repeat->first);
    auto cnt_t = analyze(node.repeat->second);
    if (cnt_t && !is_integer(*cnt_t))
      throw TypeError("array repeat count must be integer");
    if (elem_t)
      last_type = LiteralType::slice(*elem_t);
    else
      last_type.reset();
    return;
  }
  std::optional<LiteralType> elem;
  for (const auto &e : node.elements) {
    auto t = analyze(e);
    if (!t)
      continue;
    if (!elem)
      elem = t;
    else if (*elem != *t)
      throw TypeError("array elements must have the same type");
  }
  if (elem)
    last_type = LiteralType::slice(*elem);
  else
    last_type.reset();
}

inline void ExprAnalyzer::visit(IndexExpression &node) {
  auto target_t = analyze(node.target);
  auto ty = analyze(node.index);
  if (!target_t) {
    last_type.reset();
    return;
  }

  if (!ty || !is_integer(*ty)) {
    throw TypeError("Index expression requires integer index");
  }

  if (target_t->is_array()) {
    last_type = *target_t->as_array().element;
  } else if (target_t->is_slice()) {
    last_type = *target_t->as_slice().element;
  } else {
    last_type.reset();
  }
}

inline void ExprAnalyzer::visit(TupleExpression &node) {
  // No Tuple Expr anymore, just ignore
  (void)node;
}

inline void ExprAnalyzer::visit(BreakExpression &node) {
  if (node.expr)
    analyze(node.expr.value());
  last_type = LiteralType::base(PrimitiveLiteralType::NEVER);
}

inline void ExprAnalyzer::visit(ContinueExpression &) {
  last_type = LiteralType::base(PrimitiveLiteralType::NEVER);
}

inline void ExprAnalyzer::visit(PathExpression &node) {
  // No need to check here
  (void)node;
}

inline void ExprAnalyzer::visit(QualifiedPathExpression &node) {
  // No QualifiedPathExpr.
  (void)node;
}

// Statements
inline void ExprAnalyzer::visit(BlockStatement &node) {
  enterScope(ScopeKind::Block, &node, "block");
  for (const auto &st : node.statements) {
    if (st)
      st->accept(*this);
  }
  exitScope();
}

inline void ExprAnalyzer::visit(LetStatement &node) {
  if (node.expr) {
    auto expr_type = analyze(node.expr);
    if (expr_type != node.type) {
      throw TypeError("Incompatible types in let statement");
    }
  }

  if (node.pattern) {
    node.pattern->accept(*this);
  }
}

inline void ExprAnalyzer::visit(ExpressionStatement &node) {
  if (node.expression)
    node.expression->accept(*this);
}

inline void ExprAnalyzer::visit(EmptyStatement &) {}

// Patterns
inline void ExprAnalyzer::visit(IdentifierPattern &) {}

inline void ExprAnalyzer::visit(LiteralPattern &) {}

inline void ExprAnalyzer::visit(WildcardPattern &) {}

inline void ExprAnalyzer::visit(RestPattern &) {}

inline void ExprAnalyzer::visit(ReferencePattern &node) {
  if (node.inner_pattern)
    node.inner_pattern->accept(*this);
}

inline void ExprAnalyzer::visit(StructPattern &node) {
  for (const auto &field : node.fields) {
    if (field.pattern)
      field.pattern->accept(*this);
  }
}

inline void ExprAnalyzer::visit(TuplePattern &node) {
  for (const auto &el : node.elements) {
    if (el)
      el->accept(*this);
  }
}

inline void ExprAnalyzer::visit(GroupedPattern &node) {
  if (node.inner_pattern)
    node.inner_pattern->accept(*this);
}

inline void ExprAnalyzer::visit(PathPattern &) {}

inline void ExprAnalyzer::visit(SlicePattern &node) {
  for (const auto &el : node.elements) {
    if (el)
      el->accept(*this);
  }
}

inline void ExprAnalyzer::visit(OrPattern &node) {
  for (const auto &alt : node.alternatives) {
    if (alt)
      alt->accept(*this);
  }
}

// Top-level
inline void ExprAnalyzer::visit(FunctionDecl &node) {
  if (node.body) {
    enterScope(ScopeKind::Function, &node, node.name);
    node.body.value()->accept(*this);
    exitScope();
  }
}

inline void ExprAnalyzer::visit(ConstantItem &node) {
  if (node.value)
    node.value.value()->accept(*this);
}

inline void ExprAnalyzer::visit(ModuleDecl &node) {
  if (node.items) {
    enterScope(ScopeKind::Module, &node, node.name);
    for (const auto &child : *node.items) {
      if (child)
        child->accept(*this);
    }
    exitScope();
  }
}

inline void ExprAnalyzer::visit(StructDecl &) {}

inline void ExprAnalyzer::visit(EnumDecl &) {}

inline void ExprAnalyzer::visit(TraitDecl &node) {
  enterScope(ScopeKind::Trait, &node, node.name);
  for (const auto &item : node.associated_items) {
    if (item)
      item->accept(*this);
  }
  exitScope();
}

inline void ExprAnalyzer::visit(ImplDecl &node) {
  enterScope(ScopeKind::Impl, &node, "impl");
  for (const auto &item : node.associated_items) {
    if (item)
      item->accept(*this);
  }
  exitScope();
}

inline std::optional<LiteralType> ExprAnalyzer::result() const {
  return last_type;
}

inline std::optional<LiteralType>
ExprAnalyzer::literalToType(const LiteralExpression &lit) {
  return lit.type;
}

inline void ExprAnalyzer::enterScope(ScopeKind kind, const BaseNode *owner,
                                     const std::string &label) {
  symbol_table.enterScope(kind, owner, label);
}

inline void ExprAnalyzer::exitScope() { symbol_table.exitScope(); }

inline std::optional<LiteralType>
ExprAnalyzer::resolveNameType(const std::string &name) {
  auto symbol = symbol_table.lookup(name);
  if (!symbol) {
    return std::nullopt;
  }

  switch (symbol->kind) {
  case SymbolKind::Variable:
  case SymbolKind::Constant:
  case SymbolKind::Param:
    return symbol->type;
  case SymbolKind::Function:
    if (symbol->function_sig) {
      return symbol->function_sig->return_type;
    }
    return std::nullopt;
  case SymbolKind::Struct:
  case SymbolKind::Enum:
    return LiteralType::path({name});
  default:
    return std::nullopt;
  }
}

inline std::optional<LiteralType> ExprAnalyzer::resolveFunctionCallType(
    const std::string &name, const std::vector<LiteralType> &arg_types) {
  auto symbol = symbol_table.lookup(name);
  if (!symbol || symbol->kind != SymbolKind::Function) {
    return std::nullopt;
  }

  if (!symbol->function_sig) {
    return std::nullopt;
  }

  const auto &func_sig = *symbol->function_sig;

  // Check parameter count
  if (func_sig.parameters) {
    const auto &params = *func_sig.parameters;
    if (params.size() != arg_types.size()) {
      throw TypeError("Function '" + name + "' expects " +
                      std::to_string(params.size()) + " arguments, got " +
                      std::to_string(arg_types.size()));
    }

    // Check parameter types
    for (size_t i = 0; i < params.size(); ++i) {
      if (!(arg_types[i] == params[i].second)) {
        throw TypeError("Function '" + name + "' parameter " +
                        std::to_string(i) + " expects type '" +
                        to_string(params[i].second) + "', got '" +
                        to_string(arg_types[i]) + "'");
      }
    }
  } else if (!arg_types.empty()) {
    throw TypeError("Function '" + name + "' expects no arguments, got " +
                    std::to_string(arg_types.size()));
  }

  return func_sig.return_type;
}

} // namespace rc
