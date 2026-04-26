#pragma once

#include "ast/nodes/top_level.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "semantic/utils/self_replace.hpp"
#include "utils/logger.hpp"

#include <unordered_set>

namespace rc {

class ThirdPassPromoter : public BaseVisitor {
public:
  ThirdPassPromoter() = default;

  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_);

  void visit(BaseNode &node) override;
  void visit(FunctionDecl &node) override;
  void visit(ImplDecl &node) override;
  void visit(TraitDecl &) override;
  void visit(BlockExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(RootNode &) override;

private:
  ScopeNode *root_scope = nullptr;
  std::vector<ScopeNode *> scope_stack;

  ScopeNode *current_scope() const;
  void enter_scope(ScopeNode *s);
  void exit_scope();
  CollectedItem *resolve_struct(const std::string &name);
};

// Implementation















} // namespace rc
