#pragma once

#include "base.hpp"
#include "ast/types.hpp"
#include "lexer/lexer.hpp"

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

class LiteralExpression : public Expression {
public:
  std::string value;
  LiteralType type;

  LiteralExpression(std::string v, LiteralType t) : value(std::move(v)), type(t) {}

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
  std::shared_ptr<Expression> function_name;
  std::vector<std::shared_ptr<Expression>> arguments;

  CallExpression(std::shared_ptr<Expression> fname, std::vector<std::shared_ptr<Expression>> args)
      : function_name(std::move(fname)), arguments(std::move(args)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class MethodCallExpression : public Expression {
public:
  struct PathExprSegment {
    std::string name;
    std::optional<std::vector<std::shared_ptr<Expression>>> args;
  };

  std::shared_ptr<Expression> receiver;
  PathExprSegment method_name;
  std::vector<std::shared_ptr<Expression>> arguments;

  MethodCallExpression(std::shared_ptr<Expression> recv, PathExprSegment method,
                       std::vector<std::shared_ptr<Expression>> args)
      : receiver(std::move(recv)), method_name(std::move(method)),
        arguments(std::move(args)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class FieldAccessExpression : public Expression {
public:
  std::shared_ptr<Expression> target;
  std::string field_name;

  FieldAccessExpression(std::shared_ptr<Expression> t, std::string f)
      : target(std::move(t)), field_name(std::move(f)) {}

  void accept(BaseVisitor &visitor) override {
    visitor.visit(*this);
  }
};

class ArrayExpression : public Expression {
public:
  // Two forms: [e1, e2, ...] or [expr ; size]
  std::vector<std::shared_ptr<Expression>> elements; // when not repeated
  std::optional<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> repeat; // (expr ; count)

  ArrayExpression(std::vector<std::shared_ptr<Expression>> elems)
      : elements(std::move(elems)), repeat(std::nullopt) {}
  ArrayExpression(std::shared_ptr<Expression> value, std::shared_ptr<Expression> size)
      : elements(), repeat(std::make_pair(std::move(value), std::move(size))) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class IndexExpression : public Expression {
public:
  std::shared_ptr<Expression> target;
  std::shared_ptr<Expression> index;

  IndexExpression(std::shared_ptr<Expression> t, std::shared_ptr<Expression> i)
      : target(std::move(t)), index(std::move(i)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class TupleExpression : public Expression {
public:
  std::vector<std::shared_ptr<Expression>> elements;
  explicit TupleExpression(std::vector<std::shared_ptr<Expression>> elems)
      : elements(std::move(elems)) {}
  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
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

class LoopExpression : public Expression {
public:
  std::shared_ptr<Expression> body; // BlockExpression

  explicit LoopExpression(std::shared_ptr<Expression> b) : body(std::move(b)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class WhileExpression : public Expression {
public:
  std::shared_ptr<Expression> condition;
  std::shared_ptr<Expression> body; // BlockExpression

  WhileExpression(std::shared_ptr<Expression> cond, std::shared_ptr<Expression> b)
      : condition(std::move(cond)), body(std::move(b)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc