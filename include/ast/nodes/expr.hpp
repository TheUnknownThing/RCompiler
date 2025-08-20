#pragma once

#include "ast/types.hpp"
#include "base.hpp"
#include "lexer/lexer.hpp"
#include "ast/nodes/pattern.hpp"

#include <map>
#include <memory>
#include <optional>
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

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class BreakExpression : public Expression {
public:
  std::optional<std::shared_ptr<Expression>> expr;

  BreakExpression(std::optional<std::shared_ptr<Expression>> e = std::nullopt)
      : expr(std::move(e)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ContinueExpression : public Expression {
public:
  ContinueExpression() {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class LiteralExpression : public Expression {
public:
  std::string value;
  LiteralType type;

  LiteralExpression(std::string v, LiteralType t)
      : value(std::move(v)), type(t) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class PrefixExpression : public Expression {
public:
  rc::Token op;
  std::shared_ptr<Expression> right;

  PrefixExpression(rc::Token o, std::shared_ptr<Expression> r)
      : op(std::move(o)), right(std::move(r)) {}
  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class BinaryExpression : public Expression {
public:
  std::shared_ptr<Expression> left;
  rc::Token op;
  std::shared_ptr<Expression> right;

  BinaryExpression(std::shared_ptr<Expression> l, rc::Token o,
                   std::shared_ptr<Expression> r)
      : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class GroupExpression : public Expression {
public:
  std::shared_ptr<Expression> inner;

  explicit GroupExpression(std::shared_ptr<Expression> e)
      : inner(std::move(e)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class IfExpression : public Expression {
public:
  std::shared_ptr<Expression> condition;
  std::shared_ptr<Expression> then_block;
  std::optional<std::shared_ptr<Expression>> else_block;

  IfExpression(
      std::shared_ptr<Expression> cond, std::shared_ptr<Expression> then_expr,
      std::optional<std::shared_ptr<Expression>> else_expr = std::nullopt)
      : condition(std::move(cond)), then_block(std::move(then_expr)),
        else_block(std::move(else_expr)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class MatchExpression : public Expression {
public:
  struct MatchArm {
    std::shared_ptr<BasePattern> pattern;
    std::optional<std::shared_ptr<Expression>> guard;
    std::shared_ptr<Expression> body;
  };

  std::shared_ptr<Expression> scrutinee;
  std::vector<MatchArm> arms;

  MatchExpression(std::shared_ptr<Expression> expr,
                  std::vector<MatchArm> match_arms)
      : scrutinee(std::move(expr)), arms(std::move(match_arms)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ReturnExpression : public Expression {
public:
  std::optional<std::shared_ptr<Expression>> value;

  ReturnExpression(
      std::optional<std::shared_ptr<Expression>> val = std::nullopt)
      : value(std::move(val)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class CallExpression : public Expression {
public:
  std::shared_ptr<Expression> function_name;
  std::vector<std::shared_ptr<Expression>> arguments;

  CallExpression(std::shared_ptr<Expression> fname,
                 std::vector<std::shared_ptr<Expression>> args)
      : function_name(std::move(fname)), arguments(std::move(args)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
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

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class FieldAccessExpression : public Expression {
public:
  std::shared_ptr<Expression> target;
  std::string field_name;

  FieldAccessExpression(std::shared_ptr<Expression> t, std::string f)
      : target(std::move(t)), field_name(std::move(f)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ArrayExpression : public Expression {
public:
  // Two forms: [e1, e2, ...] or [expr ; size]
  std::vector<std::shared_ptr<Expression>> elements; // when not repeated
  std::optional<
      std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>>
      repeat; // (expr ; count)

  ArrayExpression(std::vector<std::shared_ptr<Expression>> elems)
      : elements(std::move(elems)), repeat(std::nullopt) {}
  ArrayExpression(std::shared_ptr<Expression> value,
                  std::shared_ptr<Expression> size)
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

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class StructExpression : public Expression {
public:
  struct FieldInit {
    std::string name;
    std::optional<std::shared_ptr<Expression>> value;
  };

  std::shared_ptr<Expression> path_expr;
  std::vector<FieldInit> fields;

  StructExpression(std::shared_ptr<Expression> path,
                   std::vector<FieldInit> inits)
      : path_expr(std::move(path)), fields(std::move(inits)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class BlockExpression : public Expression {
public:
  std::vector<std::shared_ptr<BaseNode>> statements;
  std::optional<std::shared_ptr<Expression>> final_expr;

  BlockExpression(
      std::vector<std::shared_ptr<BaseNode>> stmts,
      std::optional<std::shared_ptr<Expression>> final = std::nullopt)
      : statements(std::move(stmts)), final_expr(std::move(final)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
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

  WhileExpression(std::shared_ptr<Expression> cond,
                  std::shared_ptr<Expression> b)
      : condition(std::move(cond)), body(std::move(b)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

struct PathSegmentExprArg {
  std::vector<std::shared_ptr<Expression>> args;
};

class PathExpression : public Expression {
public:
  bool leading_colons;
  struct Segment {
    std::string ident;
    std::optional<PathSegmentExprArg> call;
  };
  std::vector<Segment> segments;

  PathExpression(bool leading, std::vector<Segment> segs)
      : leading_colons(leading), segments(std::move(segs)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class QualifiedPathExpression : public Expression {
public:
  rc::LiteralType base_type;
  // TODO: This represents TypePath, need FIX here.
  std::optional<std::vector<std::string>> as_type_path;
  std::vector<PathExpression::Segment> segments;

  QualifiedPathExpression(rc::LiteralType base,
                          std::optional<std::vector<std::string>> as_path,
                          std::vector<PathExpression::Segment> segs)
      : base_type(std::move(base)), as_type_path(std::move(as_path)),
        segments(std::move(segs)) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc