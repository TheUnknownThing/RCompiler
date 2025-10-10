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

inline void DirtyWorkPass::run(const std::shared_ptr<RootNode> &root,
                               ScopeNode *root_scope_) {
  root_scope = root_scope_;
  current_scope_node = root_scope;
  current_function_name = std::nullopt;

  LOG_DEBUG("[DirtyWorkPass] Starting dirty work pass");

  if (root) {
    for (const auto &child : root->children) {
      if (child) {
        child->accept(*this);
      }
    }
  }

  LOG_DEBUG("[DirtyWorkPass] Completed dirty work pass");
}

inline void DirtyWorkPass::visit(BaseNode &node) {
  if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*decl);
  } else if (auto *expr = dynamic_cast<CallExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ReturnExpression *>(&node)) {
    visit(*expr);
  } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *decl = dynamic_cast<ImplDecl *>(&node)) {
    visit(*decl);
  }
}

inline void DirtyWorkPass::visit(FunctionDecl &node) {
  LOG_DEBUG("[DirtyWorkPass] Entering function '" + node.name + "'");

  auto prev_function_name = current_function_name;
  auto prev_found_exit_call = found_exit_call;
  current_function_name = node.name;
  found_exit_call = false;

  auto *parent_scope = current_scope_node;
  auto *fn_scope = parent_scope->find_child_scope_by_owner(&node);
  if (fn_scope) {
    current_scope_node = fn_scope;
  }

  if (node.body && node.body.value()) {
    node.body.value()->accept(*this);
  }

  if (node.name == "main" && current_scope_node == root_scope) {
    if (!found_exit_call) {
      throw SemanticException("main function must have exit() call");
    }
  }

  current_scope_node = parent_scope;
  current_function_name = prev_function_name;
  found_exit_call = prev_found_exit_call;

  LOG_DEBUG("[DirtyWorkPass] Exiting function '" + node.name + "'");
}

inline void DirtyWorkPass::visit(CallExpression &node) {
  auto *nameExpr = dynamic_cast<NameExpression *>(node.function_name.get());
  if (nameExpr && nameExpr->name == "exit") {
    if (!current_function_name.has_value() ||
        current_function_name.value() != "main" ||
        current_scope_node != root_scope || found_exit_call || is_in_impl) {
      throw SemanticException(
          "exit() function can only be called once at the end of main");
    }

    found_exit_call = true;
  }

  for (const auto &arg : node.arguments) {
    if (arg) {
      arg->accept(*this);
    }
  }
}

inline void DirtyWorkPass::visit(BlockExpression &node) {
  LOG_DEBUG("[DirtyWorkPass] Entering block expression");

  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }

  if (current_function_name.has_value() &&
      current_function_name.value() == "main" &&
      current_scope_node == root_scope) {

    if (found_exit_call) {
      bool is_exit_final = false;

      if (node.final_expr.has_value()) {
        auto *final_call =
            dynamic_cast<CallExpression *>(node.final_expr.value().get());
        if (final_call) {
          auto *final_name =
              dynamic_cast<NameExpression *>(final_call->function_name.get());
          if (final_name && final_name->name == "exit") {
            is_exit_final = true;
          }
        }
      } else if (!node.statements.empty()) {
        auto *last_stmt =
            dynamic_cast<ExpressionStatement *>(node.statements.back().get());
        if (last_stmt && last_stmt->expression) {
          auto *last_call =
              dynamic_cast<CallExpression *>(last_stmt->expression.get());
          if (last_call) {
            auto *last_name =
                dynamic_cast<NameExpression *>(last_call->function_name.get());
            if (last_name && last_name->name == "exit") {
              is_exit_final = true;
            }
          }
        }
      }

      if (!is_exit_final) {
        throw SemanticException(
            "exit() must be the final statement in main function");
      }
    }
  }

  if (node.final_expr.has_value()) {
    node.final_expr.value()->accept(*this);
  }

  LOG_DEBUG("[DirtyWorkPass] Exiting block expression");
}

inline void DirtyWorkPass::visit(IfExpression &node) {
  if (node.condition) {
    node.condition->accept(*this);
  }
  if (node.then_block) {
    node.then_block->accept(*this);
  }
  if (node.else_block.has_value()) {
    node.else_block.value()->accept(*this);
  }
}

inline void DirtyWorkPass::visit(LoopExpression &node) {
  if (node.body) {
    node.body->accept(*this);
  }
}

inline void DirtyWorkPass::visit(WhileExpression &node) {
  if (node.condition) {
    node.condition->accept(*this);
  }
  if (node.body) {
    node.body->accept(*this);
  }
}

inline void DirtyWorkPass::visit(ReturnExpression &node) {
  if (node.value.has_value()) {
    node.value.value()->accept(*this);
  }
}

inline void DirtyWorkPass::visit(ExpressionStatement &node) {
  if (node.expression) {
    node.expression->accept(*this);
  }
}

inline void DirtyWorkPass::visit(LetStatement &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
}

inline void DirtyWorkPass::visit(ImplDecl &node) {
  LOG_DEBUG("[DirtyWorkPass] Visiting impl block");
  is_in_impl = true;
  for (const auto &item : node.associated_items) {
    if (item) {
      item->accept(*this);
    }
  }
  is_in_impl = false;
}

} // namespace rc
