#pragma once

#include "base.hpp"
#include "../types.hpp"
#include "../../lexer/lexer.hpp"

#include <map>
#include <string>
#include <vector>

namespace rc {
class Expression : public BaseNode {
public:
  virtual void accept(BaseVisitor &visitor) = 0;
};

class LiteralExpr : public Expression {
public:
  Token tok;

  LiteralExpr(const Token &token) : tok(token) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class IdentifierExpr : public Expression {
public:
  Token name;

  IdentifierExpr(const Token &nameTok) : name(nameTok) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class BinaryExpr : public Expression {
public:
  BaseNode *left;
  BaseNode *right;
  Token op;

  BinaryExpr(BaseNode *l, BaseNode *r, const Token &opTok)
      : left(l), right(r), op(opTok) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class UnaryExpr : public Expression {
public:
  BaseNode *operand;
  Token op;

  UnaryExpr(BaseNode *opnd, const Token &opTok)
      : operand(opnd), op(opTok) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class CallExpr : public Expression {
public:
  BaseNode *callee;
  std::vector<BaseNode *> arguments;

  CallExpr(BaseNode *c, const std::vector<BaseNode *> &args)
      : callee(c), arguments(args) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class AssignmentExpr : public Expression {
public:
  BaseNode *target;
  BaseNode *value;

  AssignmentExpr(BaseNode *t, BaseNode *v) : target(t), value(v) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class IfExpr : public Expression {
public:
  BaseNode *condition;
  BaseNode *then_branch;
  BaseNode *else_branch;

  IfExpr(BaseNode *cond, BaseNode *then_br, BaseNode *else_br)
      : condition(cond), then_branch(then_br), else_branch(else_br) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ReturnExpr : public Expression {
public:
  BaseNode *value;

  ReturnExpr(BaseNode *val) : value(val) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class StructExpr : public Expression {
public:
  Token name;
  std::vector<std::pair<Token, BaseNode *>> fields;

  StructExpr(const Token &n,
             const std::vector<std::pair<Token, BaseNode *>> &f)
      : name(n), fields(f) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc