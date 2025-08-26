#pragma once

#include <memory>
#include <string>

#include "ast/nodes/topLevel.hpp"
#include "semantic/scope.hpp"

namespace rc {

class FirstPassBuilder : public BaseVisitor {
public:
  ScopeNode *root_scope;

  FirstPassBuilder() = default;

  void build(const std::shared_ptr<RootNode> &root) {
    root_scope = new ScopeNode("", nullptr);
    current_scope = root_scope;
    if (root) {
      for (const auto &child : root->children) {
        if (child)
          child->accept(*this);
      }
    }
  }

  void visit(BaseNode &node) override {
    if (auto *fn = dynamic_cast<FunctionDecl *>(&node)) {
      visit(*fn);
    } else if (auto *cst = dynamic_cast<ConstantItem *>(&node)) {
      visit(*cst);
    } else if (auto *st = dynamic_cast<StructDecl *>(&node)) {
      visit(*st);
    } else if (auto *en = dynamic_cast<EnumDecl *>(&node)) {
      visit(*en);
    } else if (auto *tr = dynamic_cast<TraitDecl *>(&node)) {
      visit(*tr);
    } else if (auto *blk = dynamic_cast<BlockExpression *>(&node)) {
      visit(*blk);
    } else if (auto *ife = dynamic_cast<IfExpression *>(&node)) {
      visit(*ife);
    } else if (auto *lop = dynamic_cast<LoopExpression *>(&node)) {
      visit(*lop);
    } else if (auto *whl = dynamic_cast<WhileExpression *>(&node)) {
      visit(*whl);
    }
  }

  // Item visitors
  void visit(FunctionDecl &node) override {
    current_scope->add_item(node.name, ItemKind::Function, &node);

    if (node.body && node.body.value()) {
      node.body.value()->accept(*this);
    }
  }

  void visit(ConstantItem &node) override {
    current_scope->add_item(node.name, ItemKind::Constant, &node);
  }

  void visit(StructDecl &node) override {
    current_scope->add_item(node.name, ItemKind::Struct, &node);
  }

  void visit(EnumDecl &node) override {
    current_scope->add_item(node.name, ItemKind::Enum, &node);
  }

  void visit(TraitDecl &node) override {
    current_scope->add_item(node.name, ItemKind::Trait, &node);
    enterScope(current_scope, node.name);
    for (const auto &assoc : node.associated_items) {
      if (assoc)
        assoc->accept(*this);
    }
    exitScope(current_scope);
  }

  void visit(ImplDecl &node) override {
    // ignored in first pass
    (void)node;
  }

  void visit(BlockExpression &node) override {
    for (const auto &stmt : node.statements) {
      if (!stmt)
        continue;
      stmt->accept(*this);
    }
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
