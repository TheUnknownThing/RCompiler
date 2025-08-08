#pragma once

#include "base.hpp"
#include "../types.hpp"

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
  std::string value;

  LiteralExpr(const std::string &val) : value(val) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class IdentifierExpr : public Expression {
public:
  std::string name;

  IdentifierExpr(const std::string &name) : name(name) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class BinaryExpr : public Expression {
public:
  BaseNode *left;
  BaseNode *right;
  std::string op;

  BinaryExpr(BaseNode *l, BaseNode *r, const std::string &op)
      : left(l), right(r), op(op) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class UnaryExpr : public Expression {
public:
  BaseNode *operand;
  std::string op;

  UnaryExpr(BaseNode *op, const std::string &op_str)
      : operand(op), op(op_str) {}

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
  std::string name;
  std::map<std::string, BaseNode *> fields;

  StructExpr(const std::string &n,
             const std::map<std::string, BaseNode *> &f)
      : name(n), fields(f) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc