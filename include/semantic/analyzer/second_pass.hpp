#pragma once

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/pattern.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/top_level.hpp"
#include "semantic/analyzer/const_evaluator.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "semantic/utils/self_replace.hpp"
#include "utils/logger.hpp"

namespace rc {

class SecondPassResolver : public BaseVisitor {
public:
  SecondPassResolver();

  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_);

  void visit(BaseNode &node) override;
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(StructDecl &node) override;
  void visit(EnumDecl &node) override;
  void visit(TraitDecl &node) override;
  void visit(ImplDecl &node) override;
  void visit(BlockExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(LetStatement &node) override;
  void visit(ArrayExpression &node) override;
  void visit(BorrowExpression &node) override;
  void visit(DerefExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(PrefixExpression &node) override;
  void visit(ExpressionStatement &node) override;
  void visit(StructExpression &node) override;
  void visit(RootNode &) override;

private:
  ScopeNode *root_scope;
  std::vector<ScopeNode *> scope_stack;
  ConstEvaluator evaluator;

  ScopeNode *current_scope() const;
  void enter_scope(ScopeNode *s);
  void exit_scope();
  SemType resolve_type(AstType &t);
  const CollectedItem *resolve_named_item(const std::string &name) const;
  CollectedItem *lookup_current_value_item(const std::string &name,
                                           ItemKind kind);
  CollectedItem *lookup_current_type_item(const std::string &name,
                                          ItemKind kind);
};

// Implementation






























} // namespace rc
