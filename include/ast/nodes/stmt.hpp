#pragma once

#include "base.hpp"
#include "../../lexer/lexer.hpp"
#include "../types.hpp"
#include "expr.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace rc {

class Statement : public BaseNode {
public:
  virtual void accept(BaseVisitor &visitor) = 0;
};

class BlockStatement : public Statement {
public:
  std::vector<Statement *> statements;

  BlockStatement(const std::vector<Statement *> &stmts) : statements(stmts) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class LetStatement : public Statement {
public:
  std::string identifier;
  LiteralType type;
  std::shared_ptr<Expression> expr;

  LetStatement(const std::string &id, LiteralType ty, std::shared_ptr<Expression> e)
      : identifier(id), type(ty), expr(std::move(e)) {}

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