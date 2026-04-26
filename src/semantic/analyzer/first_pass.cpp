#include "semantic/analyzer/first_pass.hpp"

namespace rc {

FirstPassBuilder::FirstPassBuilder() = default;
std::unique_ptr<ScopeNode>
FirstPassBuilder::build(const std::shared_ptr<RootNode> &root) {
  LOG_INFO("[FirstPass] Building initial scope tree");

  prelude_scope_owner_ = create_prelude_scope();
  prelude_scope = prelude_scope_owner_.get();
  LOG_INFO("[FirstPass] Created prelude scope with " +
           std::to_string(prelude_scope->items().size()) +
           " builtin functions");

  root_scope = prelude_scope->add_child_scope("root", root.get());
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

  return std::move(prelude_scope_owner_);
}

void FirstPassBuilder::visit(BaseNode &node) {
  node.accept(*this);
}

void FirstPassBuilder::visit(FunctionDecl &node) {
  LOG_DEBUG("[FirstPass] Collect function '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Function, &node);

  if (node.body && node.body.value()) {
    node.body.value()->accept(*this);
  }
}

void FirstPassBuilder::visit(ConstantItem &node) {
  LOG_DEBUG("[FirstPass] Collect constant '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Constant, &node);
}

void FirstPassBuilder::visit(StructDecl &node) {
  LOG_DEBUG("[FirstPass] Collect struct '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Struct, &node);
}

void FirstPassBuilder::visit(EnumDecl &node) {
  LOG_DEBUG("[FirstPass] Collect enum '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Enum, &node);
}

void FirstPassBuilder::visit(TraitDecl &node) {
  LOG_DEBUG("[FirstPass] Collect trait '" + node.name + "'");
  current_scope->add_item(node.name, ItemKind::Trait, &node);
  enter_scope(current_scope, node.name, &node);
  LOG_DEBUG("[FirstPass] Enter trait scope '" + node.name + "'");
  for (const auto &assoc : node.associated_items) {
    if (assoc)
      assoc->accept(*this);
  }
  exit_scope(current_scope);
  LOG_DEBUG("[FirstPass] Exit trait scope '" + node.name + "'");
}

void FirstPassBuilder::visit(ImplDecl &node) {
  LOG_DEBUG("[FirstPass] Traverse impl block");
  for (const auto &assoc : node.associated_items) {
    if (!assoc)
      continue;
    if (auto *fn = dynamic_cast<FunctionDecl *>(assoc.get())) {
      if (fn->body && fn->body.value()) {
        fn->body.value()->accept(*this);
      }
    } else if (auto *cst = dynamic_cast<ConstantItem *>(assoc.get())) {
      if (cst->value) {
        cst->value.value()->accept(*this);
      }
    }
  }
}

void FirstPassBuilder::visit(LetStatement &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
}

void FirstPassBuilder::visit(ExpressionStatement &node) {
  if (node.expression) {
    node.expression->accept(*this);
  }
}

void FirstPassBuilder::visit(EmptyStatement &) {}
void FirstPassBuilder::visit(BlockExpression &node) {
  enter_scope(current_scope, "block", &node);
  LOG_DEBUG("[FirstPass] Enter block scope");
  for (const auto &stmt : node.statements) {
    if (!stmt)
      continue;
    stmt->accept(*this);
  }
  if (node.final_expr) {
    node.final_expr.value()->accept(*this);
  }
  exit_scope(current_scope);
  LOG_DEBUG("[FirstPass] Exit block scope");
}

void FirstPassBuilder::visit(IfExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

void FirstPassBuilder::visit(LoopExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

void FirstPassBuilder::visit(WhileExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.body)
    node.body->accept(*this);
}

void FirstPassBuilder::visit(BinaryExpression &node) {
  if (node.left)
    node.left->accept(*this);
  if (node.right)
    node.right->accept(*this);
}

void FirstPassBuilder::visit(PrefixExpression &node) {
  if (node.right)
    node.right->accept(*this);
}

void FirstPassBuilder::visit(ReturnExpression &node) {
  if (node.value)
    node.value.value()->accept(*this);
}

void FirstPassBuilder::visit(StructExpression &node) {
  if (node.path_expr)
    node.path_expr->accept(*this);
  for (const auto &field : node.fields) {
    if (field.value)
      field.value.value()->accept(*this);
  }
}

void FirstPassBuilder::visit(CallExpression &node) {
  if (node.function_name)
    node.function_name->accept(*this);
  for (const auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

void FirstPassBuilder::visit(RootNode &node) {
  for (const auto &child : node.children) {
    if (child)
      child->accept(*this);
  }
}

} // namespace rc
