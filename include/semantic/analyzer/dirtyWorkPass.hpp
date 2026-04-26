#pragma once

#include <memory>
#include <optional>
#include <string>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "utils/logger.hpp"

namespace rc {

class DirtyWorkPass : public BaseVisitor {
public:
  DirtyWorkPass() = default;

  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_);

  void visit(BaseNode &node) override;
  void visit(FunctionDecl &node) override;
  void visit(CallExpression &node) override;
  void visit(BlockExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(ReturnExpression &node) override;
  void visit(ExpressionStatement &node) override;
  void visit(LetStatement &node) override;
  void visit(ImplDecl &node) override;

private:
  ScopeNode *root_scope = nullptr;
  ScopeNode *current_scope_node = nullptr;
  std::optional<std::string> current_function_name;
  bool found_exit_call = false;
  bool is_in_impl = false;
};

// Implementation













} // namespace rc
