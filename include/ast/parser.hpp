#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "lexer/lexer.hpp"
#include "types.hpp"
#include "utils/logger.hpp"
#include "utils/parsec.hpp"
#include "utils/pratt.hpp"

#include "parsers/pattern_parser.hpp"

#include "nodes/base.hpp"
#include "nodes/expr.hpp"
#include "nodes/stmt.hpp"
#include "nodes/top_level.hpp"

namespace rc {

using namespace parsec;

class Parser {
public:
  Parser(std::vector<Token> tokens);
  std::shared_ptr<RootNode> parse();

  parsec::Parser<std::shared_ptr<Expression>> any_expression();

  parsec::Parser<std::shared_ptr<Expression>> parse_block_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_if_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_return_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_loop_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_while_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_array_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_break_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_continue_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_tuple_or_group_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_path_or_name_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_struct_expression();
  parsec::Parser<std::vector<rc::StructExpression::FieldInit>>
  parse_struct_expr_fields();

  parsec::Parser<std::shared_ptr<BaseItem>> any_top_level_item();

  parsec::Parser<std::shared_ptr<FunctionDecl>> parse_function();
  parsec::Parser<std::shared_ptr<StructDecl>> parse_struct();
  parsec::Parser<std::shared_ptr<EnumDecl>> parse_enum();
  parsec::Parser<std::shared_ptr<ConstantItem>> parse_const_item();
  parsec::Parser<std::shared_ptr<ImplDecl>> parse_impl();

  parsec::Parser<std::shared_ptr<BaseNode>> parse_let_statement();

  parsec::Parser<std::pair<std::shared_ptr<BasePattern>, AstType>>
  pattern_and_type_parser();
  parsec::Parser<
      std::vector<std::pair<std::shared_ptr<BasePattern>, AstType>>>
  argument_list_parser();
  parsec::Parser<std::pair<
      std::optional<SelfParam>,
      std::vector<std::pair<std::shared_ptr<BasePattern>, AstType>>>>
  parse_function_parameters();
  parsec::Parser<std::vector<std::shared_ptr<Expression>>>
  expression_list_parser();

  parsec::Parser<AstType> type_parser();

private:
  std::vector<Token> tokens;
  pratt::PrattTable pratt_table_;
  PatternParser pattern_parser_;

  std::shared_ptr<BaseNode> parse_statement();
};





























} // namespace rc

namespace pratt {
PrattTable default_table(rc::Parser *p);

} // namespace pratt
