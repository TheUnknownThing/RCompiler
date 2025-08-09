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

class NameExpression : public Expression {
public:
  std::string name;

  NameExpression(std::string n) : name(std::move(n)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class IntExpression : public Expression {
public:
  std::string value;

  IntExpression(std::string v) : value(std::move(v)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class PrefixExpression : public Expression {
public:
  rc::Token op;
  Expression *right;
  
  PrefixExpression(rc::Token o, Expression *r) : op(std::move(o)), right(r) {}
  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class BinaryExpression : public Expression {
public:
  Expression *left;
  rc::Token op;
  Expression *right;
  
  BinaryExpression(Expression *l, rc::Token o, Expression *r)
      : left(l), op(std::move(o)), right(r) {}
  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class GroupExpression : public Expression {
public:
  Expression *inner;

  explicit GroupExpression(Expression *e) : inner(e) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

} // namespace rc