#pragma once

#include "base.hpp"

#include <string>
#include <vector>

namespace rc {
class Expression : public nc::BaseNode {
public:
  virtual void accept(nc::BaseVisitor &visitor) = 0;
};

class LiteralExpr : public Expression {
public:
  std::string value;

  LiteralExpr(const std::string &val) : value(val) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class IdentifierExpr : public Expression {
public:
  std::string name;

  IdentifierExpr(const std::string &name) : name(name) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class BinaryExpr : public Expression {
public:
  nc::BaseNode *left;
  nc::BaseNode *right;
  std::string op;

  BinaryExpr(nc::BaseNode *l, nc::BaseNode *r, const std::string &op)
      : left(l), right(r), op(op) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class UnaryExpr : public Expression {
public:
  nc::BaseNode *operand;
  std::string op;

  UnaryExpr(nc::BaseNode *op, const std::string &op_str)
      : operand(op), op(op_str) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class CallExpr : public Expression {
public:
  nc::BaseNode *callee;
  std::vector<nc::BaseNode *> arguments;

  CallExpr(nc::BaseNode *c, const std::vector<nc::BaseNode *> &args)
      : callee(c), arguments(args) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class AssignmentExpr : public Expression {
public:
  nc::BaseNode *target;
  nc::BaseNode *value;

  AssignmentExpr(nc::BaseNode *t, nc::BaseNode *v) : target(t), value(v) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc