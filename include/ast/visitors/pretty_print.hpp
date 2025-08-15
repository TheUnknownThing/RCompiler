#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "ast/nodes/base.hpp"
#include "ast/nodes/expr.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "ast/types.hpp"

namespace rc {

class PrettyPrintVisitor : public BaseVisitor {
public:
  explicit PrettyPrintVisitor(int indent_level = 0);

  // Output the pretty-printed result
  std::string get_result() const;
  void reset();

  // Base visitor method
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
  void visit(BasePattern &node) override;
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
  std::ostringstream output_;
  int indent_level_;

  void print_indent();
  void increase_indent();
  void decrease_indent();
  void print_line(const std::string &text);
  void print_inline(const std::string &text);

  // Helper methods
  std::string format_type(const LiteralType &type);
  std::string format_token(const Token &token);
};

// Utility function to pretty print any AST node
std::string pretty_print(BaseNode &node, int indent_level = 0);

} // namespace rc
