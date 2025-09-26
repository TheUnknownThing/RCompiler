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

  void build(const std::shared_ptr<RootNode> &root);

  void visit(BaseNode &node) override;

  // Item visitors
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(StructDecl &node) override;
  void visit(EnumDecl &node) override;
  void visit(TraitDecl &node) override;

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
  void visit(RootNode &) override;

private:
  ScopeNode *current_scope = nullptr;
};

// Implementation

inline FirstPassBuilder::FirstPassBuilder() = default;

inline void FirstPassBuilder::build(const std::shared_ptr<RootNode> &root) {
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

inline void FirstPassBuilder::visit(BaseNode &node) {
  // Items
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
  }
  // Statements
  else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<EmptyStatement *>(&node)) {
    visit(*stmt);
  }
  // Expressions
  else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BinaryExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<PrefixExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ReturnExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<StructExpression *>(&node)) {
    visit(*expr);
  }
}

inline void FirstPassBuilder::visit(FunctionDecl &node) {
  LOG_DEBUG("[FirstPass] Collect function '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Function, &node);

  if (node.body && node.body.value()) {
    node.body.value()->accept(*this);
  }
}

inline void FirstPassBuilder::visit(ConstantItem &node) {
  LOG_DEBUG("[FirstPass] Collect constant '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Constant, &node);
}

inline void FirstPassBuilder::visit(StructDecl &node) {
  LOG_DEBUG("[FirstPass] Collect struct '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Struct, &node);
}

inline void FirstPassBuilder::visit(EnumDecl &node) {
  LOG_DEBUG("[FirstPass] Collect enum '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Enum, &node);
}

inline void FirstPassBuilder::visit(TraitDecl &node) {
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

inline void FirstPassBuilder::visit(LetStatement &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
}

inline void FirstPassBuilder::visit(ExpressionStatement &node) {
  if (node.expression) {
    node.expression->accept(*this);
  }
}

inline void FirstPassBuilder::visit(EmptyStatement &) {}

inline void FirstPassBuilder::visit(BlockExpression &node) {
  enterScope(current_scope, "block", &node);
  LOG_DEBUG("[FirstPass] Enter block scope");
  for (const auto &stmt : node.statements) {
    if (!stmt)
      continue;
    stmt->accept(*this);
  }
  if (node.final_expr) {
    node.final_expr.value()->accept(*this);
  }
  exitScope(current_scope);
  LOG_DEBUG("[FirstPass] Exit block scope");
}

inline void FirstPassBuilder::visit(IfExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

inline void FirstPassBuilder::visit(LoopExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

inline void FirstPassBuilder::visit(WhileExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.body)
    node.body->accept(*this);
}

inline void FirstPassBuilder::visit(BinaryExpression &node) {
  if (node.left)
    node.left->accept(*this);
  if (node.right)
    node.right->accept(*this);
}

inline void FirstPassBuilder::visit(PrefixExpression &node) {
  if (node.right)
    node.right->accept(*this);
}

inline void FirstPassBuilder::visit(ReturnExpression &node) {
  if (node.value)
    node.value.value()->accept(*this);
}

inline void FirstPassBuilder::visit(StructExpression &node) {
  if (node.path_expr)
    node.path_expr->accept(*this);
  for (const auto &field : node.fields) {
    if (field.value)
      field.value.value()->accept(*this);
  }
}

inline void FirstPassBuilder::visit(RootNode &) {}

} // namespace rc