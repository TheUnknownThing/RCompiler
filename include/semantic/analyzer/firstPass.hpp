#pragma once

#include <memory>
#include <string>

#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/builtin.hpp"
#include "semantic/scope.hpp"
#include "utils/logger.hpp"

namespace rc {

class FirstPassBuilder : public BaseVisitor {
public:
  ScopeNode *prelude_scope;
  ScopeNode *root_scope;

  FirstPassBuilder() = default;

  void build(const std::shared_ptr<RootNode> &root) {
    LOG_INFO("[FirstPass] Building initial scope tree");

    prelude_scope = create_prelude_scope();
    LOG_INFO("[FirstPass] Created prelude scope with " +
             std::to_string(prelude_scope->items().size()) +
             " builtin functions");

    root_scope = new ScopeNode("root", prelude_scope, root.get());
    prelude_scope->add_child_scope("root", root.get());
    current_scope = root_scope;
    if (root) {
      size_t idx = 0;
      for (const auto &child : root->children) {
        if (child) {
          LOG_DEBUG("[FirstPass] Visiting top-level child #" +
                    std::to_string(idx));
          child->accept(*this);
        }
        ++idx;
      }
    }
    LOG_INFO("[FirstPass] Completed. Root has " +
             std::to_string(root_scope->items().size()) + " items");
  }

  void visit(BaseNode &node) override {
    if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
      visit(*decl);
    } else if (auto *decl = dynamic_cast<StructDecl *>(&node)) {
      visit(*decl);
    } else if (auto *cst = dynamic_cast<ConstantItem *>(&node)) {
      visit(*cst);
    } else if (auto *decl = dynamic_cast<EnumDecl *>(&node)) {
      visit(*decl);
    } else if (auto *decl = dynamic_cast<TraitDecl *>(&node)) {
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

  // Item visitors
  void visit(FunctionDecl &node) override {
    LOG_DEBUG("[FirstPass] Collect function '" + node.name + "'");
    current_scope->add_item(node.name, ItemKind::Function, &node);

    if (node.body && node.body.value()) {
      node.body.value()->accept(*this);
    }
  }

  void visit(ConstantItem &node) override {
    LOG_DEBUG("[FirstPass] Collect constant '" + node.name + "'");
    current_scope->add_item(node.name, ItemKind::Constant, &node);
  }

  void visit(StructDecl &node) override {
    LOG_DEBUG("[FirstPass] Collect struct '" + node.name + "'");
    current_scope->add_item(node.name, ItemKind::Struct, &node);
  }

  void visit(EnumDecl &node) override {
    LOG_DEBUG("[FirstPass] Collect enum '" + node.name + "'");
    current_scope->add_item(node.name, ItemKind::Enum, &node);
  }

  void visit(TraitDecl &node) override {
    LOG_DEBUG("[FirstPass] Collect trait '" + node.name + "'");
    current_scope->add_item(node.name, ItemKind::Trait, &node);
    enterScope(current_scope, node.name, &node);
    LOG_DEBUG("[FirstPass] Enter trait scope '" + node.name + "'");
    for (const auto &assoc : node.associated_items) {
      if (assoc)
        assoc->accept(*this);
    }
    exitScope(current_scope);
    LOG_DEBUG("[FirstPass] Exit trait scope '" + node.name + "'");
  }

  void visit(BlockExpression &node) override {
    enterScope(current_scope, "block", &node);
    LOG_DEBUG("[FirstPass] Enter block scope");
    for (const auto &stmt : node.statements) {
      if (!stmt)
        continue;
      stmt->accept(*this);
    }
    exitScope(current_scope);
    LOG_DEBUG("[FirstPass] Exit block scope");
    if (node.final_expr) {
      node.final_expr.value()->accept(*this);
    }
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
  ScopeNode *current_scope = nullptr;
};

} // namespace rc
