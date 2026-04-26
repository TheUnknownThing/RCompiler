#pragma once

/**
 * @details This class CHECKS:
 * Inappropriate breaks and continues
 */

#include "ast/nodes/stmt.hpp"
#include "ast/nodes/top_level.hpp"
#include "semantic/error/exceptions.hpp"
#include "utils/logger.hpp"

#include <memory>

namespace rc {

class ControlAnalyzer : public BaseVisitor {
public:
  explicit ControlAnalyzer();

  bool analyze(const std::shared_ptr<BaseNode> &node);

  void visit(BaseNode &node) override;

  // Expression visitors
  void visit(PrefixExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(GroupExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(ReturnExpression &node) override;
  void visit(CallExpression &node) override;
  void visit(MethodCallExpression &node) override;
  void visit(FieldAccessExpression &node) override;
  void visit(StructExpression &node) override;
  void visit(BlockExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(ArrayExpression &node) override;
  void visit(IndexExpression &node) override;
  void visit(TupleExpression &node) override;
  void visit(BreakExpression &node) override;
  void visit(ContinueExpression &node) override;
  void visit(PathExpression &node) override;

  // Statement visitors
  void visit(LetStatement &node) override;
  void visit(ExpressionStatement &node) override;

  // Pattern visitors
  void visit(ReferencePattern &node) override;
  void visit(OrPattern &node) override;

  // Top-level declaration visitors
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(TraitDecl &node) override;
  void visit(ImplDecl &node) override;
  void visit(RootNode &node) override;

private:
  std::vector<AstType> function_return_stack;
  std::size_t loop_depth;
};

// Implementation




// Expression visitors





















// Pattern visitors


// Top-level declaration visitors





} // namespace rc
