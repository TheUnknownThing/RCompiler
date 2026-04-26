#pragma once

#include <memory>
#include <string>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/builtin.hpp"
#include "semantic/scope.hpp"
#include "utils/logger.hpp"

namespace rc {

class FirstPassBuilder : public BaseVisitor {
public:
  ScopeNode *prelude_scope;
  ScopeNode *root_scope;

  FirstPassBuilder();

  std::unique_ptr<ScopeNode> build(const std::shared_ptr<RootNode> &root);

  void visit(BaseNode &node) override;

  // Item visitors
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(StructDecl &node) override;
  void visit(EnumDecl &node) override;
  void visit(TraitDecl &node) override;
  void visit(ImplDecl &node) override;

  // Statement visitors
  void visit(LetStatement &node) override;
  void visit(ExpressionStatement &node) override;
  void visit(EmptyStatement &) override;

  // Expression visitors
  void visit(BlockExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(PrefixExpression &node) override;
  void visit(ReturnExpression &node) override;
  void visit(StructExpression &node) override;
  void visit(CallExpression &node) override;
  void visit(RootNode &) override;

private:
  ScopeNode *current_scope = nullptr;
  std::unique_ptr<ScopeNode> prelude_scope_owner_;
};

// Implementation























} // namespace rc
