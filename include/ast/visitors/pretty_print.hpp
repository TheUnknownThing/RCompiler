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

// Color constants for pretty printing
namespace Colors {
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string DIM = "\033[2m";

// Node type colors
const std::string NODE_NAME = "\033[1;34m";  // Bold Blue
const std::string FIELD_NAME = "\033[1;32m"; // Bold Green
const std::string TYPE_NAME = "\033[1;33m";  // Bold Yellow
const std::string LITERAL = "\033[1;35m";    // Bold Magenta
const std::string OPERATOR = "\033[1;31m";   // Bold Red
const std::string IDENTIFIER = "\033[36m";   // Cyan
const std::string KEYWORD = "\033[1;37m";    // Bold White

// Structural colors
const std::string BRACE = "\033[37m";     // White
const std::string BRACKET = "\033[37m";   // White
const std::string SEPARATOR = "\033[90m"; // Dark Gray
} // namespace Colors

class PrettyPrintVisitor : public BaseVisitor {
public:
  explicit PrettyPrintVisitor(int indent_level = 0, bool use_colors = true);

  // Output the pretty-printed result
  std::string get_result() const;
  void reset();

  // Enable/disable color output
  void set_colors(bool enabled) { use_colors_ = enabled; }

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
  void visit(StructExpression &node) override;
  void visit(BreakExpression &node) override;
  void visit(ContinueExpression &node) override;
  void visit(PathExpression &node) override;
  void visit(QualifiedPathExpression &node) override;
  void visit(BorrowExpression &node) override;
  void visit(DerefExpression &node) override;

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
  bool use_colors_;
  bool in_list_context_; // Track if we're currently inside a list

  // Core printing methods
  void print_indent();
  void increase_indent();
  void decrease_indent();
  void print_line(const std::string &text);
  void print_inline(const std::string &text);
  void print_newline();

  // Enhanced formatting methods
  void print_node_start(const std::string &node_name);
  void print_node_start_inline(const std::string &node_name); // No indent
  void print_node_end();
  void print_node_end_inline(); // No indent
  void print_field(const std::string &field_name, const std::string &value);
  void print_field_start(const std::string &field_name);
  void print_field_end();
  void print_list_start(const std::string &list_name);
  void print_list_end();
  void print_list_item_start();
  void print_list_item_end();

  // Color helper methods
  std::string colorize(const std::string &text, const std::string &color) const;
  std::string node_color(const std::string &text) const;
  std::string field_color(const std::string &text) const;
  std::string type_color(const std::string &text) const;
  std::string literal_color(const std::string &text) const;
  std::string operator_color(const std::string &text) const;
  std::string identifier_color(const std::string &text) const;

  // Helper methods
  std::string format_type(const LiteralType &type);
  std::string format_token(const Token &token);
};

// Utility function to pretty print any AST node
std::string pretty_print(BaseNode &node, int indent_level = 0);

} // namespace rc
