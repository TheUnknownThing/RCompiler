#include "semantic/analyzer/control_analyzer.hpp"

namespace rc {

ControlAnalyzer::ControlAnalyzer() : loop_depth(0) {}
bool ControlAnalyzer::analyze(const std::shared_ptr<BaseNode> &node) {
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

void ControlAnalyzer::visit(BaseNode &node) {
  node.accept(*this);
}

void ControlAnalyzer::visit(PrefixExpression &node) {
  if (node.right)
    node.right->accept(*this);
}

void ControlAnalyzer::visit(BinaryExpression &node) {
  if (node.left)
    node.left->accept(*this);
  if (node.right)
    node.right->accept(*this);
}

void ControlAnalyzer::visit(GroupExpression &node) {
  if (node.inner)
    node.inner->accept(*this);
}

void ControlAnalyzer::visit(IfExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

void ControlAnalyzer::visit(ReturnExpression &) {
  if (function_return_stack.empty()) {
    throw SemanticException("return outside of function");
  }
}

void ControlAnalyzer::visit(CallExpression &node) {
  if (node.function_name)
    node.function_name->accept(*this);
  for (auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

void ControlAnalyzer::visit(MethodCallExpression &node) {
  if (node.receiver)
    node.receiver->accept(*this);
  for (auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

void ControlAnalyzer::visit(FieldAccessExpression &node) {
  if (node.target)
    node.target->accept(*this);
}

void ControlAnalyzer::visit(StructExpression &node) {
  if (node.path_expr)
    node.path_expr->accept(*this);
  for (auto &f : node.fields) {
    if (f.value)
      f.value.value()->accept(*this);
  }
}

void ControlAnalyzer::visit(BlockExpression &node) {
  for (auto &s : node.statements) {
    if (s)
      s->accept(*this);
  }
  if (node.final_expr)
    node.final_expr.value()->accept(*this);
}

void ControlAnalyzer::visit(LoopExpression &node) {
  loop_depth++;
  if (node.body)
    node.body->accept(*this);
  loop_depth--;
}

void ControlAnalyzer::visit(WhileExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  loop_depth++;
  if (node.body)
    node.body->accept(*this);
  loop_depth--;
}

void ControlAnalyzer::visit(ArrayExpression &node) {
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

void ControlAnalyzer::visit(IndexExpression &node) {
  if (node.target)
    node.target->accept(*this);
  if (node.index)
    node.index->accept(*this);
}

void ControlAnalyzer::visit(TupleExpression &node) {
  for (auto &e : node.elements) {
    if (e)
      e->accept(*this);
  }
}

void ControlAnalyzer::visit(BreakExpression &node) {
  if (loop_depth == 0) {
    throw SemanticException("break outside of loop");
  }
  if (node.expr)
    node.expr.value()->accept(*this);
}

void ControlAnalyzer::visit(ContinueExpression &node) {
  if (loop_depth == 0) {
    throw SemanticException("continue outside of loop");
  }
  (void)node;
}

void ControlAnalyzer::visit(PathExpression &node) {
  for (auto &seg : node.segments) {
    if (seg.call) {
      for (auto &arg : seg.call->args) {
        if (arg)
          arg->accept(*this);
      }
    }
  }
}

void ControlAnalyzer::visit(LetStatement &node) {
  if (node.pattern)
    node.pattern->accept(*this);
  if (node.expr)
    node.expr->accept(*this);
}

void ControlAnalyzer::visit(ExpressionStatement &node) {
  if (node.expression)
    node.expression->accept(*this);
}

void ControlAnalyzer::visit(ReferencePattern &node) {
  if (node.inner_pattern)
    node.inner_pattern->accept(*this);
}

void ControlAnalyzer::visit(OrPattern &node) {
  for (auto &alt : node.alternatives) {
    if (alt)
      alt->accept(*this);
  }
}

void ControlAnalyzer::visit(FunctionDecl &node) {
  function_return_stack.push_back(node.return_type);
  if (node.body) {
    node.body.value()->accept(*this);
  }
  function_return_stack.pop_back();
}

void ControlAnalyzer::visit(ConstantItem &node) {
  if (node.value)
    node.value.value()->accept(*this);
}

void ControlAnalyzer::visit(TraitDecl &node) {
  for (auto &item : node.associated_items) {
    if (item)
      item->accept(*this);
  }
}

void ControlAnalyzer::visit(ImplDecl &node) {
  for (auto &item : node.associated_items) {
    if (item)
      item->accept(*this);
  }
}

void ControlAnalyzer::visit(RootNode &node) {
  for (auto &child : node.children) {
    if (child)
      child->accept(*this);
  }
}

} // namespace rc
