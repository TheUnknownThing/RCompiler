#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/constEvaluator.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "utils/logger.hpp"

namespace rc {

class ConstEvaluationPass : public BaseVisitor {
public:
  ConstEvaluationPass() = default;

  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_) {
    LOG_INFO("[ConstEvaluationPass] Starting constant evaluation");
    root_scope = root_scope_;
    scope_stack.clear();
    scope_stack.push_back(root_scope);

    if (root) {
      size_t idx = 0;
      for (const auto &child : root->children) {
        if (child) {
          LOG_DEBUG("[ConstEvaluationPass] Evaluating top-level item #" +
                    std::to_string(idx));
          child->accept(*this);
        }
        ++idx;
      }
    }
    LOG_INFO("[ConstEvaluationPass] Completed");
  }

  void visit(BaseNode &node) override {
    if (auto *cst = dynamic_cast<ConstantItem *>(&node)) {
      visit(*cst);
    } else if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
      visit(*decl);
    } else if (auto *decl = dynamic_cast<StructDecl *>(&node)) {
      visit(*decl);
    } else if (auto *decl = dynamic_cast<EnumDecl *>(&node)) {
      visit(*decl);
    } else if (auto *decl = dynamic_cast<TraitDecl *>(&node)) {
      visit(*decl);
    } else if (auto *decl = dynamic_cast<ImplDecl *>(&node)) {
      visit(*decl);
    } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
      visit(*expr);
    }
  }

  void visit(ConstantItem &node) override {
    LOG_DEBUG("[ConstEvaluationPass] Evaluating constant '" + node.name + "'");

    auto *ci = lookup_current_value_item(node.name, ItemKind::Constant);
    if (!ci) {
      LOG_ERROR("[ConstEvaluationPass] Constant metadata not found for '" +
                node.name + "'");
      return;
    }

    auto &meta = ci->as_constant_meta();

    // only trait constant do not contain initializer
    if (node.value) {
      try {
        auto evaluated = evaluator.evaluate(node.value->get(), current_scope());
        if (evaluated) {
          meta.evaluated_value =
              std::make_shared<ConstValue>(std::move(*evaluated));
          LOG_DEBUG("[ConstEvaluationPass] Successfully evaluated constant '" +
                    node.name + "'");
        } else {
          throw SemanticException(
              "[ConstEvaluationPass] Could not evaluate constant '" +
              node.name + "' - not a constant expression");
        }
      } catch (const std::exception &e) {
        LOG_ERROR("[ConstEvaluationPass] Error evaluating constant '" +
                  node.name + "': " + e.what());
        throw;
      }
    }
  }

  void visit(FunctionDecl &node) override {
    if (node.body && node.body.value()) {
      node.body.value()->accept(*this);
    }
  }

  void visit(StructDecl &) override {}

  void visit(EnumDecl &) override {}

  void visit(TraitDecl &node) override {
    auto *parent_scope = current_scope();
    auto *trait_scope = parent_scope->find_child_scope_by_owner(&node);

    LOG_DEBUG("[ConstEvaluationPass] Enter trait '" + node.name + "'");
    push_scope(trait_scope);

    for (const auto &assoc : node.associated_items) {
      if (assoc)
        assoc->accept(*this);
    }

    pop_scope();
    LOG_DEBUG("[ConstEvaluationPass] Exit trait '" + node.name + "'");
  }

  void visit(ImplDecl &node) override {
    for (const auto &assoc : node.associated_items) {
      if (assoc)
        assoc->accept(*this);
    }
  }

  void visit(BlockExpression &node) override {
    auto *block_scope = current_scope()->find_child_scope_by_owner(&node);
    push_scope(block_scope);

    for (const auto &stmt : node.statements) {
      if (stmt)
        stmt->accept(*this);
    }

    if (node.final_expr)
      node.final_expr.value()->accept(*this);

    pop_scope();
  }

  void visit(IfExpression &node) override {
    if (node.then_block)
      node.then_block->accept(*this);
    if (node.else_block)
      node.else_block.value()->accept(*this);
  }

  void visit(LoopExpression &node) override {
    if (node.body)
      node.body->accept(*this);
  }

  void visit(WhileExpression &node) override {
    if (node.body)
      node.body->accept(*this);
  }

  void visit(RootNode &) override {}

private:
  ScopeNode *root_scope = nullptr;
  std::vector<ScopeNode *> scope_stack;
  ConstEvaluator evaluator;

  ScopeNode *current_scope() const { return scope_stack.back(); }

  void push_scope(ScopeNode *s) {
    if (s) {
      scope_stack.push_back(s);
    }
  }

  void pop_scope() {
    if (scope_stack.size() > 1)
      scope_stack.pop_back();
  }

  CollectedItem *lookup_current_value_item(const std::string &name,
                                           ItemKind kind) {
    auto *scope = current_scope();
    auto *found = scope->find_value_item(name);
    if (found && found->kind == kind)
      return found;
    throw SemanticException("item " + name + " not found in value namespace");
  }
};

} // namespace rc
