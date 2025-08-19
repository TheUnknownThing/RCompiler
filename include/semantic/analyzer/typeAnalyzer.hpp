#pragma once

#include "ast/nodes/base.hpp"
#include "ast/nodes/expr.hpp"
#include "ast/nodes/pattern.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"

namespace rc {

class SemanticContext;

class TypeAnalyzer : public BaseVisitor {
public:
  explicit TypeAnalyzer(SemanticContext &ctx);

  void check(const std::shared_ptr<RootNode> &root);

  void visit(BaseNode &node) override;

  // Expression visitors
  void visit(NameExpression &) override;
  void visit(LiteralExpression &) override;
  void visit(PrefixExpression &) override;
  void visit(BinaryExpression &) override;
  void visit(GroupExpression &) override;
  void visit(IfExpression &) override;
  void visit(MatchExpression &) override;
  void visit(ReturnExpression &) override;
  void visit(CallExpression &) override;
  void visit(MethodCallExpression &) override;
  void visit(FieldAccessExpression &) override;
  void visit(UnderscoreExpression &) override;
  void visit(BlockExpression &) override;
  void visit(LoopExpression &) override;
  void visit(WhileExpression &) override;
  void visit(ArrayExpression &) override;
  void visit(IndexExpression &) override;
  void visit(TupleExpression &) override;
  void visit(BreakExpression &) override;
  void visit(ContinueExpression &) override;
  void visit(PathExpression &) override;
  void visit(QualifiedPathExpression &) override;

  // Statement visitors
  void visit(BlockStatement &) override;
  void visit(LetStatement &) override;
  void visit(ExpressionStatement &) override;
  void visit(EmptyStatement &) override;

  // Pattern visitors
  void visit(IdentifierPattern &) override;
  void visit(LiteralPattern &) override;
  void visit(WildcardPattern &) override;
  void visit(RestPattern &) override;
  void visit(ReferencePattern &) override;
  void visit(StructPattern &) override;
  void visit(TuplePattern &) override;
  void visit(GroupedPattern &) override;
  void visit(PathPattern &) override;
  void visit(SlicePattern &) override;
  void visit(OrPattern &) override;

  // Top-level declaration visitors
  void visit(FunctionDecl &) override;
  void visit(ConstantItem &) override;
  void visit(ModuleDecl &) override;
  void visit(StructDecl &) override;
  void visit(EnumDecl &) override;
  void visit(TraitDecl &) override;
  void visit(ImplDecl &) override;
  void visit(RootNode &) override;

private:
  SemanticContext &ctx;
};

inline TypeAnalyzer::TypeAnalyzer(SemanticContext &ctx) : ctx(ctx) {}

inline void TypeAnalyzer::check(const std::shared_ptr<RootNode> &root) {
  visit(*root);
}

inline void TypeAnalyzer::visit(BaseNode &node) {
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

    // Statements
  } else if (auto *stmt = dynamic_cast<BlockStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<EmptyStatement *>(&node)) {
    visit(*stmt);

    // Top-level
  } else if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
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

    // Patterns
  } else if (auto *p = dynamic_cast<IdentifierPattern *>(&node)) {
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

  } else if (auto *root_node = dynamic_cast<RootNode *>(&node)) {
    visit(*root_node);
  } else {
    // No-op
  }
}

inline void TypeAnalyzer::visit(RootNode &) {}

// Expressions
inline void TypeAnalyzer::visit(NameExpression &) {}

inline void TypeAnalyzer::visit(LiteralExpression &) {}

inline void TypeAnalyzer::visit(PrefixExpression &) {}

inline void TypeAnalyzer::visit(BinaryExpression &) {}

inline void TypeAnalyzer::visit(GroupExpression &) {}

inline void TypeAnalyzer::visit(IfExpression &) {}

inline void TypeAnalyzer::visit(MatchExpression &) {}

inline void TypeAnalyzer::visit(ReturnExpression &) {}

inline void TypeAnalyzer::visit(CallExpression &) {}

inline void TypeAnalyzer::visit(MethodCallExpression &) {}

inline void TypeAnalyzer::visit(FieldAccessExpression &) {}

inline void TypeAnalyzer::visit(UnderscoreExpression &) {}

inline void TypeAnalyzer::visit(BlockExpression &) {}

inline void TypeAnalyzer::visit(LoopExpression &) {}

inline void TypeAnalyzer::visit(WhileExpression &) {}

inline void TypeAnalyzer::visit(ArrayExpression &) {}

inline void TypeAnalyzer::visit(IndexExpression &) {}

inline void TypeAnalyzer::visit(TupleExpression &) {}

inline void TypeAnalyzer::visit(BreakExpression &) {}

inline void TypeAnalyzer::visit(ContinueExpression &) {}

inline void TypeAnalyzer::visit(PathExpression &) {}

inline void TypeAnalyzer::visit(QualifiedPathExpression &) {}

// Statements
inline void TypeAnalyzer::visit(BlockStatement &) {}

inline void TypeAnalyzer::visit(LetStatement &) {}

inline void TypeAnalyzer::visit(ExpressionStatement &) {}

inline void TypeAnalyzer::visit(EmptyStatement &) {}

// Patterns
inline void TypeAnalyzer::visit(IdentifierPattern &) {}

inline void TypeAnalyzer::visit(LiteralPattern &) {}

inline void TypeAnalyzer::visit(WildcardPattern &) {}

inline void TypeAnalyzer::visit(RestPattern &) {}

inline void TypeAnalyzer::visit(ReferencePattern &) {}

inline void TypeAnalyzer::visit(StructPattern &) {}

inline void TypeAnalyzer::visit(TuplePattern &) {}

inline void TypeAnalyzer::visit(GroupedPattern &) {}

inline void TypeAnalyzer::visit(PathPattern &) {}

inline void TypeAnalyzer::visit(SlicePattern &) {}

inline void TypeAnalyzer::visit(OrPattern &) {}

// Top-level
inline void TypeAnalyzer::visit(FunctionDecl &) {}

inline void TypeAnalyzer::visit(ConstantItem &) {}

inline void TypeAnalyzer::visit(ModuleDecl &) {}

inline void TypeAnalyzer::visit(StructDecl &) {}

inline void TypeAnalyzer::visit(EnumDecl &) {}

inline void TypeAnalyzer::visit(TraitDecl &) {}

inline void TypeAnalyzer::visit(ImplDecl &) {}

} // namespace rc
