#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "nodes/base.hpp"
#include "nodes/expr.hpp"
#include "nodes/stmt.hpp"
#include "nodes/topLevel.hpp"
#include "types.hpp"

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
  void visit(NameExpression &node);
  void visit(IntExpression &node);
  void visit(PrefixExpression &node);
  void visit(BinaryExpression &node);
  void visit(GroupExpression &node);
  void visit(IfExpression &node);
  void visit(MatchExpression &node);
  void visit(ReturnExpression &node);
  void visit(UnderscoreExpression &node);
  void visit(BlockExpression &node);

  // Statement visitors
  void visit(BlockStatement &node);
  void visit(LetStatement &node);
  void visit(ExpressionStatement &node);
  void visit(EmptyStatement &node);

  // Top-level declaration visitors
  void visit(FunctionDecl &node);
  void visit(ConstantItem &node);
  void visit(ModuleDecl &node);
  void visit(StructDecl &node);
  void visit(EnumDecl &node);
  void visit(TraitDecl &node);
  void visit(ImplDecl &node);
  void visit(RootNode &node);

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
