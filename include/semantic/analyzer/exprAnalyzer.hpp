#pragma once

#include "ast/nodes/expr.hpp"
#include "semantic/analyzer/symbolTable.hpp"

#include <memory>
#include <optional>

namespace rc {

class SemanticContext;

class ExprAnalyzer : public BaseVisitor {
public:
  explicit ExprAnalyzer(SemanticContext &ctx);

  std::optional<LiteralType> analyze(const std::shared_ptr<Expression> &expr);

  void visit(BaseNode &node) override;

  void visit(NameExpression &) override;
  void visit(LiteralExpression &) override;
  void visit(PrefixExpression &) override;
  void visit(BinaryExpression &) override;
  void visit(GroupExpression &) override;
  void visit(IfExpression &) override;
  void visit(MatchExpression &) override;
  void visit(ReturnExpression &) override;
  void visit(CallExpression &) override;
  void visit(FieldAccessExpression &) override;
  void visit(MethodCallExpression &) override;
  void visit(UnderscoreExpression &) override;
  void visit(BlockExpression &) override;
  void visit(LoopExpression &) override;
  void visit(WhileExpression &) override;
  void visit(BreakExpression &) override;
  void visit(ContinueExpression &) override;
  void visit(ArrayExpression &) override;
  void visit(IndexExpression &) override;
  void visit(TupleExpression &) override;
  void visit(PathExpression &) override;
  void visit(QualifiedPathExpression &) override;

  std::optional<LiteralType> result() const;

private:
  SemanticContext &ctx_;
  std::optional<LiteralType> last_type_;

  std::optional<LiteralType> evaluateBinary(const LiteralType &lhs,
                                            const rc::Token &op,
                                            const LiteralType &rhs);
  std::optional<LiteralType> literalToType(const LiteralExpression &lit);
};

} // namespace rc
