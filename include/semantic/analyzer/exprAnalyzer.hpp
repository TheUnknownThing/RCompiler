#pragma once

/**
 * @note THIS CLASS IS DEPRECATED! EXPR CHECK WOULD BE IMPLEMENTED LATER!
 * @details This class CHECKS:
 * 1. Operator compatibility in Expressions
 * 2. Type compatibility in let stmt, const item, and return
 */

#include "ast/nodes/expr.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"

#include <memory>
#include <optional>

namespace rc {

class SemanticContext;

class ExprAnalyzer : public BaseVisitor {
public:
  explicit ExprAnalyzer();

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
  std::optional<LiteralType> last_type;

  std::optional<LiteralType> evaluateBinary(const LiteralType &lhs,
                                            const rc::Token &op,
                                            const LiteralType &rhs);
  std::optional<LiteralType> literalToType(const LiteralExpression &lit);
};

inline ExprAnalyzer::ExprAnalyzer(): last_type(std::nullopt) {}

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

} // namespace rc
