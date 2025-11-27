#pragma once

#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/utils/self_replace.hpp"
#include "semantic/types.hpp"
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
  void enterScope(ScopeNode *s);
  void exitScope();
  CollectedItem *resolve_struct(const std::string &name);
};

// Implementation

inline void ThirdPassPromoter::run(const std::shared_ptr<RootNode> &root,
                                   ScopeNode *root_scope_) {
  LOG_INFO("[ThirdPass] Promoting impl blocks");
  root_scope = root_scope_;
  if (!root_scope)
    throw SemanticException("third pass requires root scope");
  scope_stack.clear();
  scope_stack.push_back(root_scope);
  if (root) {
    size_t idx = 0;
    for (const auto &child : root->children) {
      if (child) {
        LOG_DEBUG("[ThirdPass] Visiting top-level item #" +
                  std::to_string(idx));
        child->accept(*this);
      }
      ++idx;
    }
  }
  LOG_INFO("[ThirdPass] Completed");
}

inline void ThirdPassPromoter::visit(BaseNode &node) {
  if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
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

inline void ThirdPassPromoter::visit(FunctionDecl &node) {
  if (node.body && node.body.value()) {
    auto expr = node.body.value();
    expr->accept(*this);
  }
}

inline void ThirdPassPromoter::visit(ImplDecl &node) {
  LOG_DEBUG("[ThirdPass] Impl block for target type start");
  if (node.impl_type != ImplDecl::ImplType::Inherent) {
    // Trait impls not handled
    return;
  }

  std::string target_name;
  if (node.target_type.is_path()) {
    const auto &segs = node.target_type.as_path().segments;
    if (segs.size() == 1) {
      target_name = segs[0];
    } else {
      // not support qualified path
      return;
    }
  } else {
    throw SemanticException("unsupported target type");
  }

  CollectedItem *struct_item = resolve_struct(target_name);
  if (!struct_item) {
    throw SemanticException("impl target " + target_name + " not found");
  }
  if (!struct_item->has_struct_meta()) {
    throw SemanticException("impl target " + target_name + " is not struct");
  }
  auto &meta = struct_item->as_struct_meta();

  std::unordered_set<std::string> existing;

  for (const auto &m : meta.methods)
    existing.insert(m.name);
  for (const auto &c : meta.constants)
    existing.insert(c.name);

  for (const auto &assoc : node.associated_items) {
    if (!assoc)
      continue;
      if (auto *fn = dynamic_cast<FunctionDecl *>(assoc.get())) {
        if (existing.contains(fn->name)) {
          throw SemanticException("duplicate name " + fn->name + " in impl");
        }
        existing.insert(fn->name);
        FunctionMetaData fmd;
      fmd.name = fn->name;
        if (fn->params) {
          for (const auto &p : *fn->params) {
            fmd.param_names.push_back(p.first);
            auto ty = replace_self(p.second, target_name);
            fmd.param_types.push_back(
                ScopeNode::resolve_type(ty, current_scope()));
          }
        }
        auto ret_ty = replace_self(fn->return_type, target_name);
        fmd.return_type = ScopeNode::resolve_type(ret_ty, current_scope());
        fmd.decl = fn;
        meta.methods.push_back(std::move(fmd));
        LOG_DEBUG("[ThirdPass] Added method '" + fn->name + "' to struct '" +
                  target_name + "'");
      } else if (auto *cst = dynamic_cast<ConstantItem *>(assoc.get())) {
      if (existing.contains(cst->name)) {
        throw SemanticException("duplicate name " + cst->name + " in impl");
      }
      existing.insert(cst->name);
      ConstantMetaData cmd;
      cmd.name = cst->name;
      auto ty = replace_self(cst->type, target_name);
      cmd.type = ScopeNode::resolve_type(ty, current_scope());
      cmd.decl = cst;
      meta.constants.push_back(std::move(cmd));
      LOG_DEBUG("[ThirdPass] Added constant '" + cst->name + "' to struct '" +
                target_name + "'");
    }
  }
}

inline void ThirdPassPromoter::visit(TraitDecl &) {
  // TODO: I do not want to care about it now.
}

inline void ThirdPassPromoter::visit(BlockExpression &node) {
  auto *block_scope = current_scope()->find_child_scope_by_owner(&node);
  enterScope(block_scope);
  for (const auto &stmt : node.statements) {
    if (stmt)
      stmt->accept(*this);
  }
  exitScope();
  if (node.final_expr)
    node.final_expr.value()->accept(*this);
}

inline void ThirdPassPromoter::visit(IfExpression &node) {
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

inline void ThirdPassPromoter::visit(LoopExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

inline void ThirdPassPromoter::visit(WhileExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

inline void ThirdPassPromoter::visit(RootNode &) {}

inline ScopeNode *ThirdPassPromoter::current_scope() const {
  return scope_stack.back();
}

inline void ThirdPassPromoter::enterScope(ScopeNode *s) {
  scope_stack.push_back(s);
}

inline void ThirdPassPromoter::exitScope() {
  if (scope_stack.size() > 1)
    scope_stack.pop_back();
}

inline CollectedItem *
ThirdPassPromoter::resolve_struct(const std::string &name) {
  for (auto *scope = current_scope(); scope; scope = scope->parent) {
    if (auto *ci = scope->find_type_item(name)) {
      if (ci->kind == ItemKind::Struct)
        return ci;
    }
  }
  return nullptr;
}

} // namespace rc
