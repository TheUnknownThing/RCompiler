#pragma once

/**
 * @details This class CHECKS:
 * 1. Inappropriate breaks and continues
 * 2. Return type compatibility (also expr evaluate type)
 */

#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/exprAnalyzer.hpp"
#include "semantic/analyzer/symbolTable.hpp"

#include <memory>

namespace rc {

class SemanticContext;

class StmtAnalyzer : public BaseVisitor {
public:
  explicit StmtAnalyzer(SymbolTable &symbols);

  bool analyze(const std::shared_ptr<Statement> &stmt);

  bool analyzeNode(const std::shared_ptr<BaseNode> &node);

  void visit(BaseNode &node) override;

  // Expression visitors
  void visit(NameExpression &node) override;
  void visit(LiteralExpression &node) override;
  void visit(PrefixExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(GroupExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(MatchExpression &node) override;
  void visit(ReturnExpression &node) override;
  void visit(CallExpression &node) override;
  void visit(MethodCallExpression &node) override;
  void visit(FieldAccessExpression &node) override;
  void visit(StructExpression &node) override;
  void visit(UnderscoreExpression &node) override;
  void visit(BlockExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(ArrayExpression &node) override;
  void visit(IndexExpression &node) override;
  void visit(TupleExpression &node) override;
  void visit(BreakExpression &node) override;
  void visit(ContinueExpression &node) override;
  void visit(PathExpression &node) override;
  void visit(QualifiedPathExpression &node) override;

  // Statement visitors
  void visit(BlockStatement &node) override;
  void visit(LetStatement &node) override;
  void visit(ExpressionStatement &node) override;
  void visit(EmptyStatement &node) override;

  // Pattern visitors
  void visit(IdentifierPattern &node) override;
  void visit(LiteralPattern &node) override;
  void visit(WildcardPattern &node) override;
  void visit(RestPattern &node) override;
  void visit(ReferencePattern &node) override;
  void visit(StructPattern &node) override;
  void visit(TuplePattern &node) override;
  void visit(GroupedPattern &node) override;
  void visit(PathPattern &node) override;
  void visit(SlicePattern &node) override;
  void visit(OrPattern &node) override;

  // Top-level declaration visitors
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(ModuleDecl &node) override;
  void visit(StructDecl &node) override;
  void visit(EnumDecl &node) override;
  void visit(TraitDecl &node) override;
  void visit(ImplDecl &node) override;
  void visit(RootNode &node) override;

private:
  SymbolTable &symbols_;
  ExprAnalyzer expr_analyzer;
  std::vector<LiteralType> function_return_stack;
  std::size_t loop_depth;
};

// Implementation

inline StmtAnalyzer::StmtAnalyzer(SymbolTable &symbols)
    : symbols_(symbols), expr_analyzer(symbols), loop_depth(0) {}

inline bool StmtAnalyzer::analyze(const std::shared_ptr<Statement> &stmt) {
  loop_depth = 0;
  function_return_stack.clear();
  if (!stmt)
    return true;
  stmt->accept(*this);
  return true;
}

inline bool StmtAnalyzer::analyzeNode(const std::shared_ptr<BaseNode> &node) {
  loop_depth = 0;
  function_return_stack.clear();
  if (!node)
    return true;
  node->accept(*this);
  return true;
}

inline void StmtAnalyzer::visit(BaseNode &node) {
  // Expressions
  if (auto *expr = dynamic_cast<NameExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LiteralExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<PrefixExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BinaryExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<GroupExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<MatchExpression *>(&node)) {
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
  } else if (auto *expr = dynamic_cast<UnderscoreExpression *>(&node)) {
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
  } else if (auto *expr = dynamic_cast<QualifiedPathExpression *>(&node)) {
    visit(*expr);
  }
  // Statements
  else if (auto *stmt = dynamic_cast<BlockStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<EmptyStatement *>(&node)) {
    visit(*stmt);
  }
  // Top-level
  else if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ConstantItem *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ModuleDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<StructDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<EnumDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<TraitDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ImplDecl *>(&node)) {
    visit(*decl);
  } else if (auto *root = dynamic_cast<RootNode *>(&node)) {
    visit(*root);
  }
  // Patterns
  else if (auto *p = dynamic_cast<IdentifierPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<LiteralPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<WildcardPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<RestPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<ReferencePattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<StructPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<TuplePattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<GroupedPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<PathPattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<SlicePattern *>(&node)) {
    visit(*p);
  } else if (auto *p = dynamic_cast<OrPattern *>(&node)) {
    visit(*p);
  } else {
    // No-op, unknown node type for this analyzer
  }
}

// Expression visitors
inline void StmtAnalyzer::visit(NameExpression &node) { (void)node; }

inline void StmtAnalyzer::visit(LiteralExpression &node) { (void)node; }

inline void StmtAnalyzer::visit(PrefixExpression &node) {
  if (node.right)
    node.right->accept(*this);
}

inline void StmtAnalyzer::visit(BinaryExpression &node) {
  if (node.left)
    node.left->accept(*this);
  if (node.right)
    node.right->accept(*this);
}

inline void StmtAnalyzer::visit(GroupExpression &node) {
  if (node.inner)
    node.inner->accept(*this);
}

inline void StmtAnalyzer::visit(IfExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

inline void StmtAnalyzer::visit(MatchExpression &node) {
  if (node.scrutinee)
    node.scrutinee->accept(*this);
  for (auto &arm : node.arms) {
    if (arm.pattern)
      arm.pattern->accept(*this);
    if (arm.guard)
      arm.guard.value()->accept(*this);
    if (arm.body)
      arm.body->accept(*this);
  }
}

inline void StmtAnalyzer::visit(ReturnExpression &node) {
  if (function_return_stack.empty()) {
    throw SemanticException("return outside of function");
  }
  auto expected = function_return_stack.back();
  LiteralType actual = LiteralType::base(PrimitiveLiteralType::NONE);
  if (node.value) {
    auto t = expr_analyzer.analyze(node.value.value());
    if (t)
      actual = *t;
  }
  if (!(actual == expected)) {
    throw TypeError("Return type mismatch: expected '" + to_string(expected) +
                    "' but got '" + to_string(actual) + "'");
  }
}

inline void StmtAnalyzer::visit(CallExpression &node) {
  if (node.function_name)
    node.function_name->accept(*this);
  for (auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

inline void StmtAnalyzer::visit(MethodCallExpression &node) {
  if (node.receiver)
    node.receiver->accept(*this);
  for (auto &arg : node.arguments) {
    if (arg)
      arg->accept(*this);
  }
}

inline void StmtAnalyzer::visit(FieldAccessExpression &node) {
  if (node.target)
    node.target->accept(*this);
}

inline void StmtAnalyzer::visit(StructExpression &node) {
  if (node.path_expr)
    node.path_expr->accept(*this);
  for (auto &f : node.fields) {
    if (f.value)
      f.value.value()->accept(*this);
  }
}

inline void StmtAnalyzer::visit(UnderscoreExpression &node) { (void)node; }

inline void StmtAnalyzer::visit(BlockExpression &node) {
  for (auto &s : node.statements) {
    if (s)
      s->accept(*this);
  }
  if (node.final_expr)
    node.final_expr.value()->accept(*this);
}

inline void StmtAnalyzer::visit(LoopExpression &node) {
  loop_depth++;
  if (node.body)
    node.body->accept(*this);
  loop_depth--;
}

inline void StmtAnalyzer::visit(WhileExpression &node) {
  if (node.condition)
    node.condition->accept(*this);
  loop_depth++;
  if (node.body)
    node.body->accept(*this);
  loop_depth--;
}

inline void StmtAnalyzer::visit(ArrayExpression &node) {
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

inline void StmtAnalyzer::visit(IndexExpression &node) {
  if (node.target)
    node.target->accept(*this);
  if (node.index)
    node.index->accept(*this);
}

inline void StmtAnalyzer::visit(TupleExpression &node) {
  for (auto &e : node.elements) {
    if (e)
      e->accept(*this);
  }
}

inline void StmtAnalyzer::visit(BreakExpression &node) {
  if (loop_depth == 0) {
    throw SemanticException("break outside of loop");
  }
  if (node.expr)
    node.expr.value()->accept(*this);
}

inline void StmtAnalyzer::visit(ContinueExpression &node) {
  if (loop_depth == 0) {
    throw SemanticException("continue outside of loop");
  }
  (void)node;
}

inline void StmtAnalyzer::visit(PathExpression &node) {
  for (auto &seg : node.segments) {
    if (seg.call) {
      for (auto &arg : seg.call->args) {
        if (arg)
          arg->accept(*this);
      }
    }
  }
}

inline void StmtAnalyzer::visit(QualifiedPathExpression &node) {
  // No QualiiedPath anymore
}

// Statement visitors
inline void StmtAnalyzer::visit(BlockStatement &node) {
  for (auto &s : node.statements) {
    if (s)
      s->accept(*this);
  }
}

inline void StmtAnalyzer::visit(LetStatement &node) {
  if (node.pattern)
    node.pattern->accept(*this);
  if (node.expr)
    node.expr->accept(*this);
}

inline void StmtAnalyzer::visit(ExpressionStatement &node) {
  if (node.expression)
    node.expression->accept(*this);
}

inline void StmtAnalyzer::visit(EmptyStatement &node) { (void)node; }

// Pattern visitors
inline void StmtAnalyzer::visit(IdentifierPattern &node) {
  if (node.subpattern)
    node.subpattern.value()->accept(*this);
}

inline void StmtAnalyzer::visit(LiteralPattern &node) { (void)node; }

inline void StmtAnalyzer::visit(WildcardPattern &node) { (void)node; }

inline void StmtAnalyzer::visit(RestPattern &node) { (void)node; }

inline void StmtAnalyzer::visit(ReferencePattern &node) {
  if (node.inner_pattern)
    node.inner_pattern->accept(*this);
}

inline void StmtAnalyzer::visit(StructPattern &node) {
  for (auto &f : node.fields) {
    if (f.pattern)
      f.pattern->accept(*this);
  }
}

inline void StmtAnalyzer::visit(TuplePattern &node) {
  for (auto &el : node.elements) {
    if (el)
      el->accept(*this);
  }
}

inline void StmtAnalyzer::visit(GroupedPattern &node) {
  if (node.inner_pattern)
    node.inner_pattern->accept(*this);
}

inline void StmtAnalyzer::visit(PathPattern &node) { (void)node; }

inline void StmtAnalyzer::visit(SlicePattern &node) {
  for (auto &el : node.elements) {
    if (el)
      el->accept(*this);
  }
}

inline void StmtAnalyzer::visit(OrPattern &node) {
  for (auto &alt : node.alternatives) {
    if (alt)
      alt->accept(*this);
  }
}

// Top-level declaration visitors
inline void StmtAnalyzer::visit(FunctionDecl &node) {
  function_return_stack.push_back(node.return_type);
  if (node.body) {
    node.body.value()->accept(*this);
  }
  function_return_stack.pop_back();
}

inline void StmtAnalyzer::visit(ConstantItem &node) {
  if (node.value)
    node.value.value()->accept(*this);
}

inline void StmtAnalyzer::visit(ModuleDecl &node) {
  if (node.items) {
    for (auto &child : *node.items) {
      if (child)
        child->accept(*this);
    }
  }
}

inline void StmtAnalyzer::visit(StructDecl &node) { (void)node; }

inline void StmtAnalyzer::visit(EnumDecl &node) { (void)node; }

inline void StmtAnalyzer::visit(TraitDecl &node) {
  for (auto &item : node.associated_items) {
    if (item)
      item->accept(*this);
  }
}

inline void StmtAnalyzer::visit(ImplDecl &node) {
  for (auto &item : node.associated_items) {
    if (item)
      item->accept(*this);
  }
}

inline void StmtAnalyzer::visit(RootNode &node) {
  for (auto &child : node.children) {
    if (child)
      child->accept(*this);
  }
}

} // namespace rc
