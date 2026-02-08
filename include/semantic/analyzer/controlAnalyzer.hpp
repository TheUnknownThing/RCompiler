#pragma once

/**
 * @details This class CHECKS:
 * Inappropriate breaks and continues
 */

#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"
#include "utils/logger.hpp"

#include <memory>

namespace rc {

class ControlAnalyzer : public BaseVisitor {
public:
  explicit ControlAnalyzer();

  bool analyze(const std::shared_ptr<BaseNode> &node);

  void visit(BaseNode &node) override;

  // Expression visitors
  void visit(PrefixExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(GroupExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(ReturnExpression &node) override;
  void visit(CallExpression &node) override;
  void visit(MethodCallExpression &node) override;
  void visit(FieldAccessExpression &node) override;
  void visit(StructExpression &node) override;
  void visit(BlockExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(ArrayExpression &node) override;
  void visit(IndexExpression &node) override;
  void visit(TupleExpression &node) override;
  void visit(BreakExpression &node) override;
  void visit(ContinueExpression &node) override;
  void visit(PathExpression &node) override;

  // Statement visitors
  void visit(LetStatement &node) override;
  void visit(ExpressionStatement &node) override;

  // Pattern visitors
  void visit(ReferencePattern &node) override;
  void visit(OrPattern &node) override;

  // Top-level declaration visitors
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(TraitDecl &node) override;
  void visit(ImplDecl &node) override;
  void visit(RootNode &node) override;

private:
  std::vector<LiteralType> function_return_stack;
  std::size_t loop_depth;
};

// Implementation

inline ControlAnalyzer::ControlAnalyzer() : loop_depth(0) {}

inline bool ControlAnalyzer::analyze(const std::shared_ptr<BaseNode> &node) {
  LOG_INFO("[ControlAnalyzer] Starting control flow analysis");
  loop_depth = 0;
  function_return_stack.clear();
  if (!node) {
    LOG_WARN("[ControlAnalyzer] No AST node provided for analysis");
    return false;
  }
  node->accept(*this);
  LOG_INFO("[ControlAnalyzer] Control flow analysis completed");
  return true;
}

inline void ControlAnalyzer::visit(BaseNode &node) {
  // Expressions
  if (auto *expr = dynamic_cast<PrefixExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BinaryExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<GroupExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ReturnExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<CallExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<MethodCallExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<FieldAccessExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<StructExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ArrayExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IndexExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<TupleExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BreakExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ContinueExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<PathExpression *>(&node)) {
    visit(*expr);
  }
  // Statements
  else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*stmt);
  }
  // Top-level
  else if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ConstantItem *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<TraitDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ImplDecl *>(&node)) {
    visit(*decl);
  } else if (auto *root = dynamic_cast<RootNode *>(&node)) {
    visit(*root);
  }
  // Patterns
  else if (auto *p = dynamic_cast<ReferencePattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<OrPattern *>(&node)) {
    visit(*p);
  } else {
    // No-op, unknown node type for this analyzer
  }
}

// Expression visitors

inline void ControlAnalyzer::visit(PrefixExpression &node) {
  if (node.right)
    node.right->accept(*this);
}

inline void ControlAnalyzer::visit(BinaryExpression &node) {
  if (node.left)
    node.left->accept(*this);
  if (node.right)
    node.right->accept(*this);
}

inline void ControlAnalyzer::visit(GroupExpression &node) {
  if (node.inner)
    node.inner->accept(*this);
}

inline void ControlAnalyzer::visit(IfExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

inline void ControlAnalyzer::visit(ReturnExpression &) {
  if (function_return_stack.empty()) {
    throw SemanticException("return outside of function");
  }
}

inline void ControlAnalyzer::visit(CallExpression &node) {
  if (node.function_name)
    node.function_name->accept(*this);
  for (auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

inline void ControlAnalyzer::visit(MethodCallExpression &node) {
  if (node.receiver)
    node.receiver->accept(*this);
  for (auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

inline void ControlAnalyzer::visit(FieldAccessExpression &node) {
  if (node.target)
    node.target->accept(*this);
}

inline void ControlAnalyzer::visit(StructExpression &node) {
  if (node.path_expr)
    node.path_expr->accept(*this);
  for (auto &f : node.fields) {
    if (f.value)
      f.value.value()->accept(*this);
  }
}

inline void ControlAnalyzer::visit(BlockExpression &node) {
  for (auto &s : node.statements) {
    if (s)
      s->accept(*this);
  }
  if (node.final_expr)
    node.final_expr.value()->accept(*this);
}

inline void ControlAnalyzer::visit(LoopExpression &node) {
  loop_depth++;
  if (node.body)
    node.body->accept(*this);
  loop_depth--;
}

inline void ControlAnalyzer::visit(WhileExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  loop_depth++;
  if (node.body)
    node.body->accept(*this);
  loop_depth--;
}

inline void ControlAnalyzer::visit(ArrayExpression &node) {
  if (node.repeat) {
    if (node.repeat->first)
      node.repeat->first->accept(*this);
    if (node.repeat->second)
      node.repeat->second->accept(*this);
  } else {
    for (auto &e : node.elements) {
      if (e)
        e->accept(*this);
    }
  }
}

inline void ControlAnalyzer::visit(IndexExpression &node) {
  if (node.target)
    node.target->accept(*this);
  if (node.index)
    node.index->accept(*this);
}

inline void ControlAnalyzer::visit(TupleExpression &node) {
  for (auto &e : node.elements) {
    if (e)
      e->accept(*this);
  }
}

inline void ControlAnalyzer::visit(BreakExpression &node) {
  if (loop_depth == 0) {
    throw SemanticException("break outside of loop");
  }
  if (node.expr)
    node.expr.value()->accept(*this);
}

inline void ControlAnalyzer::visit(ContinueExpression &node) {
  if (loop_depth == 0) {
    throw SemanticException("continue outside of loop");
  }
  (void)node;
}

inline void ControlAnalyzer::visit(PathExpression &node) {
  for (auto &seg : node.segments) {
    if (seg.call) {
      for (auto &arg : seg.call->args) {
        if (arg)
          arg->accept(*this);
      }
    }
  }
}

inline void ControlAnalyzer::visit(LetStatement &node) {
  if (node.pattern)
    node.pattern->accept(*this);
  if (node.expr)
    node.expr->accept(*this);
}

inline void ControlAnalyzer::visit(ExpressionStatement &node) {
  if (node.expression)
    node.expression->accept(*this);
}

// Pattern visitors
inline void ControlAnalyzer::visit(ReferencePattern &node) {
  if (node.inner_pattern)
    node.inner_pattern->accept(*this);
}

inline void ControlAnalyzer::visit(OrPattern &node) {
  for (auto &alt : node.alternatives) {
    if (alt)
      alt->accept(*this);
  }
}

// Top-level declaration visitors
inline void ControlAnalyzer::visit(FunctionDecl &node) {
  function_return_stack.push_back(node.return_type);
  if (node.body) {
    node.body.value()->accept(*this);
  }
  function_return_stack.pop_back();
}

inline void ControlAnalyzer::visit(ConstantItem &node) {
  if (node.value)
    node.value.value()->accept(*this);
}

inline void ControlAnalyzer::visit(TraitDecl &node) {
  for (auto &item : node.associated_items) {
    if (item)
      item->accept(*this);
  }
}

inline void ControlAnalyzer::visit(ImplDecl &node) {
  for (auto &item : node.associated_items) {
    if (item)
      item->accept(*this);
  }
}

inline void ControlAnalyzer::visit(RootNode &node) {
  for (auto &child : node.children) {
    if (child)
      child->accept(*this);
  }
}

} // namespace rc
