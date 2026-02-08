#pragma once

#include "../types.hpp"
#include "ast/nodes/pattern.hpp"
#include "base.hpp"
#include "expr.hpp"

#include <memory>

namespace rc {

class Statement : public BaseNode {
public:
  virtual void accept(BaseVisitor &visitor) = 0;
};

class LetStatement : public Statement {
public:
  std::shared_ptr<BasePattern> pattern;
  LiteralType type;
  std::shared_ptr<Expression> expr;

  LetStatement(std::shared_ptr<BasePattern> pat, LiteralType ty,
               std::shared_ptr<Expression> e)
      : pattern(std::move(pat)), type(std::move(ty)), expr(std::move(e)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ExpressionStatement : public Statement {
public:
  std::shared_ptr<Expression> expression;
  bool has_semicolon;

  ExpressionStatement(std::shared_ptr<Expression> expr, bool semicolon = true)
      : expression(std::move(expr)), has_semicolon(semicolon) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class EmptyStatement : public Statement {
public:
  EmptyStatement() {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc