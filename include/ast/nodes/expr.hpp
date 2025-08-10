#pragma once

#include "base.hpp"
#include "../types.hpp"
#include "../../lexer/lexer.hpp"

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <optional>

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
  std::shared_ptr<Expression> right;
  
  PrefixExpression(rc::Token o, std::shared_ptr<Expression> r) : op(std::move(o)), right(std::move(r)) {}
  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class BinaryExpression : public Expression {
public:
  std::shared_ptr<Expression> left;
  rc::Token op;
  std::shared_ptr<Expression> right;
  
  BinaryExpression(std::shared_ptr<Expression> l, rc::Token o, std::shared_ptr<Expression> r)
      : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class GroupExpression : public Expression {
public:
  std::shared_ptr<Expression> inner;

  explicit GroupExpression(std::shared_ptr<Expression> e) : inner(std::move(e)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class IfExpression : public Expression {
public:
  std::shared_ptr<Expression> condition;
  std::shared_ptr<Expression> then_block;
  std::optional<std::shared_ptr<Expression>> else_block;

  IfExpression(std::shared_ptr<Expression> cond, 
               std::shared_ptr<Expression> then_expr,
               std::optional<std::shared_ptr<Expression>> else_expr = std::nullopt)
      : condition(std::move(cond)), then_block(std::move(then_expr)), else_block(std::move(else_expr)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class MatchExpression : public Expression {
public:
  struct MatchArm {
    std::string pattern;  // Simplified - just identifier for now
    std::optional<std::shared_ptr<Expression>> guard;
    std::shared_ptr<Expression> body;
  };

  std::shared_ptr<Expression> scrutinee;
  std::vector<MatchArm> arms;

  MatchExpression(std::shared_ptr<Expression> expr, std::vector<MatchArm> match_arms)
      : scrutinee(std::move(expr)), arms(std::move(match_arms)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class ReturnExpression : public Expression {
public:
  std::optional<std::shared_ptr<Expression>> value;

  ReturnExpression(std::optional<std::shared_ptr<Expression>> val = std::nullopt)
      : value(std::move(val)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class CallExpression : public Expression {
public:
  std::string function_name;
  std::vector<std::shared_ptr<Expression>> arguments;

  CallExpression(std::string name, std::vector<std::shared_ptr<Expression>> args)
      : function_name(std::move(name)), arguments(std::move(args)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class UnderscoreExpression : public Expression {
public:
  UnderscoreExpression() {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class BlockExpression : public Expression {
public:
  std::vector<std::shared_ptr<BaseNode>> statements;
  std::optional<std::shared_ptr<Expression>> final_expr;

  BlockExpression(std::vector<std::shared_ptr<BaseNode>> stmts, 
                  std::optional<std::shared_ptr<Expression>> final = std::nullopt)
      : statements(std::move(stmts)), final_expr(std::move(final)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

} // namespace rc