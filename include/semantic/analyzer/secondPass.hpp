#pragma once

#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/firstPass.hpp"
#include "semantic/error/exceptions.hpp"

#include <unordered_set>

namespace rc {

class SecondPassBuilder : public BaseVisitor {
public:
  SecondPassBuilder(FirstPassBuilder &firstPasspass)
      : firstPass(firstPasspass) {}

  void build(const std::shared_ptr<RootNode> &root) {
    if (!root)
      return;

    currentScope = const_cast<FirstPassScopeNode *>(firstPass.rootScope());
    scopeStack.clear();
    root->accept(*this);
  }

  void visit(BaseNode &node) override {
    if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
      visit(*decl);
    } else if (auto *root = dynamic_cast<RootNode *>(&node)) {
      visit(*root);
    } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<MatchExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
      visit(*expr);
    }
    // Other items / exprs do not consider here.
  }

  void visit(RootNode &node) override {
    for (auto &child : node.children) {
      if (child)
        child->accept(*this);
    }
  }

  void visit(FunctionDecl &node) override {
    executeWithScope(&node, [&] {
      if (node.body && *node.body) {
        (*node.body)->accept(*this);
      }
    });
  }

  void visit(TraitDecl &node) override {
    executeWithScope(&node, [&] {
      for (auto &item : node.associated_items) {
        if (item)
          item->accept(*this);
      }
    });
  }

  void visit(BlockExpression &node) override {
    executeWithScope(&node, [&] {
      for (auto &stmt : node.statements) {
        if (stmt)
          stmt->accept(*this);
      }
    });
  }

  void visit(IfExpression &node) override {
    if (node.then_block)
      node.then_block->accept(*this);
    if (node.else_block && *node.else_block)
      (*node.else_block)->accept(*this);
  }

  void visit(LoopExpression &node) override {
    if (node.body)
      node.body->accept(*this);
  }

  void visit(WhileExpression &node) override {
    if (node.body)
      node.body->accept(*this);
  }

  void visit(ImplDecl &node) override {
    if (!node.target_type.is_path() ||
        node.target_type.as_path().segments.empty()) {
      throw SemanticException("Impl target is not a valid path to a struct");
    }
    const auto &segs = node.target_type.as_path().segments;
    const std::string &struct_name = segs.back();

    FirstPassScopeNode *scope_with_struct = nullptr;
    for (FirstPassScopeNode *s = currentScope; s != nullptr; s = s->parent) {
      auto it = s->items.find(struct_name);
      if (it != s->items.end() &&
          it->second.kind == FirstPassItemKind::Struct) {
        scope_with_struct = s;
        break;
      }
    }
    if (!scope_with_struct) {
      throw SemanticException("Impl target struct not found: " + struct_name);
    }

    FirstPassItem &struct_item = scope_with_struct->items.at(struct_name);
    if (!struct_item.struct_data) {
      throw SemanticException("Target is not struct: " + struct_name);
    }

    auto &metadata = *struct_item.struct_data;
    FirstPassStruct::ImplInfo impl_info;
    impl_info.impl_decl = &node;

    for (auto &assoc : node.associated_items) {
      if (!assoc)
        continue;
      if (auto *fn = dynamic_cast<FunctionDecl *>(assoc.get())) {
        const std::string &name = fn->name;
        if (metadata.impl_method_names.count(name) ||
            metadata.impl_const_names.count(name)) {
          throw SemanticException("Duplicate " + name + " for struct " +
                                  struct_name);
        }
        FirstPassStruct::ImplMethod m;
        m.name = name;
        m.decl = fn;
        m.signature.return_type = fn->return_type;
        if (fn->params) {
          m.signature.params = *fn->params;
        }
        impl_info.methods.push_back(std::move(m));
        metadata.impl_method_names.insert(name);
      } else if (auto *cst = dynamic_cast<ConstantItem *>(assoc.get())) {
        const std::string &name = cst->name;
        if (metadata.impl_method_names.count(name) ||
            metadata.impl_const_names.count(name)) {
          throw SemanticException("Duplicate " + name + " for struct " +
                                  struct_name);
        }
        FirstPassStruct::ImplConst c;
        c.name = name;
        c.decl = cst;
        c.type = cst->type;
        impl_info.consts.push_back(std::move(c));
        metadata.impl_const_names.insert(name);
      }
    }

    metadata.impls.push_back(std::move(impl_info));
  }

private:
  FirstPassBuilder &firstPass;
  FirstPassScopeNode *currentScope = nullptr;
  std::vector<FirstPassScopeNode *> scopeStack;

  template <typename Func>
  void executeWithScope(const BaseNode *owner, Func &&fn) {
    if (!owner) {
      throw SemanticException("Owner node is null");
    }
    FirstPassScopeNode *target = firstPass.scopeFor(owner);
    if (!target) {
      throw SemanticException("Scope not found for node");
    }
    scopeStack.push_back(currentScope);
    currentScope = target;
    fn();
    currentScope = scopeStack.back();
    scopeStack.pop_back();
  }
};

} // namespace rc
