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
#include "nodes/topLevel.hpp"

using namespace parsec;

namespace rc {

class Parser {
public:
  Parser(std::vector<Token> tokens);
  std::shared_ptr<RootNode> parse();

  inline parsec::Parser<std::shared_ptr<Expression>> any_expression();

  parsec::Parser<std::shared_ptr<Expression>> parse_block_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_if_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_return_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_match_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_loop_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_while_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_array_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_break_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_continue_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_tuple_or_group_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_path_or_name_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_qualified_path_expression();
  parsec::Parser<std::vector<rc::StructExpression::FieldInit>>
  parse_struct_expr_fields();

  inline parsec::Parser<std::shared_ptr<BaseItem>> any_top_level_item();

  parsec::Parser<std::shared_ptr<FunctionDecl>> parse_function();
  parsec::Parser<std::shared_ptr<StructDecl>> parse_struct();
  parsec::Parser<std::shared_ptr<ModuleDecl>> parse_module();
  parsec::Parser<std::shared_ptr<EnumDecl>> parse_enum();
  parsec::Parser<std::shared_ptr<ConstantItem>> parse_const_item();

  parsec::Parser<std::shared_ptr<BaseNode>> parse_let_statement();

  parsec::Parser<std::pair<std::string, LiteralType>>
  identifier_and_type_parser();
  parsec::Parser<std::vector<std::pair<std::string, LiteralType>>>
  argument_list_parser();
  parsec::Parser<std::vector<std::shared_ptr<Expression>>>
  expression_list_parser();

private:
  std::vector<Token> tokens;
  pratt::PrattTable pratt_table_;
  PatternParser pattern_parser_;

  std::shared_ptr<BaseNode> parse_statement();
};

inline Parser::Parser(std::vector<Token> tokens)
    : tokens(std::move(tokens)), pratt_table_(pratt::default_table(this)),
      pattern_parser_(PatternParser()) {
  LOG_DEBUG("Parser initialized with " + std::to_string(this->tokens.size()) +
            " tokens");
}

inline parsec::Parser<std::shared_ptr<BaseItem>> Parser::any_top_level_item() {
  LOG_DEBUG("Creating parser for any top-level item");
  return parsec::Parser<std::shared_ptr<BaseItem>>(
      [this](const std::vector<Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<BaseItem>> {
        size_t saved_pos = pos;

        if (auto func = parse_function().parse(toks, pos)) {
          LOG_DEBUG("Parsed function at position " + std::to_string(saved_pos));
          return func;
        }
        pos = saved_pos;
        if (auto strct = parse_struct().parse(toks, pos)) {
          LOG_DEBUG("Parsed struct at position " + std::to_string(saved_pos));
          return strct;
        }
        pos = saved_pos;
        if (auto mod = parse_module().parse(toks, pos)) {
          LOG_DEBUG("Parsed module at position " + std::to_string(saved_pos));
          return mod;
        }
        pos = saved_pos;
        if (auto en = parse_enum().parse(toks, pos)) {
          LOG_DEBUG("Parsed enum at position " + std::to_string(saved_pos));
          return en;
        }
        pos = saved_pos;
        if (auto const_item = parse_const_item().parse(toks, pos)) {
          LOG_DEBUG("Parsed constant item at position " +
                    std::to_string(saved_pos));
          return const_item;
        }
        pos = saved_pos;
        LOG_DEBUG("No top-level item found at position " +
                  std::to_string(saved_pos));
        return std::nullopt;
      });
}

inline std::shared_ptr<RootNode> Parser::parse() {
  LOG_INFO("Starting parsing process");
  auto root = std::make_shared<RootNode>();

  // top level item parser
  auto item_parser = any_top_level_item();

  size_t pos = 0;
  int item_count = 0;
  while (pos < tokens.size() && tokens[pos].type != TokenType::TOK_EOF) {
    size_t saved = pos;
    if (auto item = item_parser.parse(tokens, pos)) {
      root->children.push_back(std::move(*item));
      item_count++;
      LOG_DEBUG("Parsed top-level item #" + std::to_string(item_count));
      continue;
    }
    pos = saved;
    LOG_ERROR("Parse error: expected a top-level item at token index " +
              std::to_string(pos) + " (token: " +
              (pos < tokens.size() ? tokens[pos].lexeme : "EOF") + ")");
    throw std::runtime_error(
        "Parse error: expected a top-level item at token index " +
        std::to_string(pos));
  }

  LOG_INFO("Parsing completed successfully. Parsed " +
           std::to_string(item_count) + " top-level items");
  return root;
}

inline std::shared_ptr<BaseNode> Parser::parse_statement() {
  LOG_DEBUG("Parsing statement");

  auto let_stmt = parse_let_statement();

  auto empty_stmt =
      tok(TokenType::SEMICOLON).map([](auto) -> std::shared_ptr<BaseNode> {
        LOG_DEBUG("Parsed empty statement");
        return std::make_shared<EmptyStatement>();
      });

  // expr ; or expr (no semicolon)
  auto expr_stmt = any_expression().combine(
      optional(tok(TokenType::SEMICOLON)), [](auto e, const auto &semi) {
        return std::shared_ptr<BaseNode>(
            new ExpressionStatement(e, semi.has_value()));
      });

  auto stmt = parsec::Parser<std::shared_ptr<BaseNode>>(
      [let_stmt, empty_stmt, expr_stmt](
          const std::vector<rc::Token> &toks,
          size_t &pos) -> parsec::ParseResult<std::shared_ptr<BaseNode>> {
        size_t saved = pos;
        if (auto r = let_stmt.parse(toks, pos))
          return r;
        pos = saved;
        if (auto r = empty_stmt.parse(toks, pos))
          return r;
        pos = saved;
        if (auto r = expr_stmt.parse(toks, pos))
          return r;
        pos = saved;
        return std::nullopt;
      });

  size_t pos = 0;
  if (auto r = stmt.parse(tokens, pos)) {
    LOG_DEBUG("Successfully parsed statement");
    return *r;
  }
  LOG_WARN("Failed to parse statement");
  return nullptr;
}

inline parsec::Parser<std::shared_ptr<FunctionDecl>> Parser::parse_function() {
  auto identifier_and_type = identifier_and_type_parser();
  auto argument_list = argument_list_parser();
  auto return_type = tok(TokenType::ARROW).thenR(typ);

  auto header =
      tok(TokenType::FN)
          .thenR(parsec::identifier)
          .combine(optional(argument_list),
                   [](const auto &name, const auto &params) {
                     LOG_DEBUG("Parsing function: " + name + " with " +
                               std::to_string(params ? params->size() : 0) +
                               " parameters");
                     return std::make_pair(name, params);
                   })
          .combine(optional(return_type), [](const auto &pm_list,
                                             const auto &ty) {
            auto ret_ty = ty.value_or(LiteralType(PrimitiveLiteralType::UNIT));
            return std::tuple<
                std::string,
                std::optional<std::vector<std::pair<std::string, LiteralType>>>,
                LiteralType>{pm_list.first, pm_list.second, ret_ty};
          });

  auto body_block = parse_block_expression().map([](auto e) {
    LOG_DEBUG("Function has block body");
    return std::optional<std::shared_ptr<Expression>>(e);
  });
  auto body_semi = tok(TokenType::SEMICOLON).map([](auto) {
    LOG_DEBUG("Function has no body (declaration only)");
    return std::optional<std::shared_ptr<Expression>>(std::nullopt);
  });

  auto body = parsec::Parser<std::optional<std::shared_ptr<Expression>>>(
      [body_block, body_semi](const std::vector<rc::Token> &toks, size_t &pos)
          -> parsec::ParseResult<std::optional<std::shared_ptr<Expression>>> {
        size_t saved = pos;
        if (auto b = body_block.parse(toks, pos))
          return *b;
        pos = saved;
        if (auto s = body_semi.parse(toks, pos))
          return *s;
        return std::nullopt;
      });

  return header.combine(body, [](auto h, auto b) {
    LOG_DEBUG("Successfully parsed function: " + std::get<0>(h));
    return std::make_shared<FunctionDecl>(std::get<0>(h), std::get<1>(h),
                                          std::get<2>(h), b);
  });
}

inline parsec::Parser<std::shared_ptr<StructDecl>> Parser::parse_struct() {
  using SD = StructDecl;

  auto field = parsec::identifier.thenL(tok(TokenType::COLON))
                   .combine(typ, [](const auto &id, const auto &t) {
                     LOG_DEBUG("Parsed struct field: " + id);
                     return std::make_pair(id, t);
                   });
  auto fields = tok(TokenType::L_BRACE)
                    .thenR(many(field.thenL(optional(tok(TokenType::COMMA)))))
                    .thenL(tok(TokenType::R_BRACE));

  auto tuple_fields =
      tok(TokenType::L_PAREN)
          .thenR(many(typ.thenL(optional(tok(TokenType::COMMA)))))
          .thenL(tok(TokenType::R_PAREN))
          .thenL(tok(TokenType::SEMICOLON));

  auto parser =
      tok(TokenType::STRUCT)
          .thenR(parsec::identifier)
          .combine(
              parsec::Parser<
                  std::variant<std::vector<std::pair<std::string, LiteralType>>,
                               std::vector<LiteralType>>>(
                  [fields, tuple_fields](const std::vector<rc::Token> &toks,
                                         size_t &pos)
                      -> parsec::ParseResult<std::variant<
                          std::vector<std::pair<std::string, LiteralType>>,
                          std::vector<LiteralType>>> {
                    size_t saved = pos;
                    if (auto f = fields.parse(toks, pos)) {
                      LOG_DEBUG("Parsed named struct fields");
                      return *f;
                    }
                    pos = saved;
                    if (auto tf = tuple_fields.parse(toks, pos)) {
                      LOG_DEBUG("Parsed tuple struct fields");
                      return *tf;
                    }
                    return std::nullopt;
                  }),
              [](const auto &name, const auto &var) {
                LOG_DEBUG("Parsing struct: " + name);
                return std::make_pair(name, var);
              })
          .map([](auto p) {
            const auto &name = p.first;
            const auto &var = p.second;
            return std::visit(
                [&](auto &&val) -> std::shared_ptr<StructDecl> {
                  using T = std::decay_t<decltype(val)>;
                  if constexpr (std::is_same_v<
                                    T, std::vector<std::pair<std::string,
                                                             LiteralType>>>) {
                    LOG_DEBUG("Successfully parsed named struct: " + name +
                              " with " + std::to_string(val.size()) +
                              " fields");
                    return std::make_shared<StructDecl>(
                        name, SD::StructType::Struct, val,
                        std::vector<LiteralType>{});
                  } else {
                    LOG_DEBUG("Successfully parsed tuple struct: " + name +
                              " with " + std::to_string(val.size()) +
                              " fields");
                    return std::make_shared<StructDecl>(
                        name, SD::StructType::Tuple,
                        std::vector<std::pair<std::string, LiteralType>>{},
                        val);
                  }
                },
                var);
          });

  return parser;
}

inline parsec::Parser<std::shared_ptr<ModuleDecl>> Parser::parse_module() {
  return parsec::Parser<std::shared_ptr<ModuleDecl>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<ModuleDecl>> {
        size_t saved = pos;
        if (!tok(TokenType::MOD).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        auto maybe_name = parsec::identifier.parse(toks, pos);
        if (!maybe_name) {
          pos = saved;
          return std::nullopt;
        }
        const std::string name = *maybe_name;

        {
          size_t sem_pos = pos;
          if (tok(TokenType::SEMICOLON).parse(toks, sem_pos)) {
            pos = sem_pos;
            LOG_DEBUG("Parsed module decl (semicolon form): " + name);
            return std::make_shared<ModuleDecl>(name, std::nullopt);
          }
        }

        if (!tok(TokenType::L_BRACE).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        std::vector<std::shared_ptr<BaseNode>> items;

        // Local item parser
        auto item_parser = any_top_level_item();

        int count = 0;
        for (;;) {
          size_t before = pos;
          if (tok(TokenType::R_BRACE).parse(toks, pos)) {
            LOG_DEBUG("Parsed module decl with " + std::to_string(count) +
                      " items: " + name);
            return std::make_shared<ModuleDecl>(name, std::move(items));
          }
          pos = before;

          if (auto item = item_parser.parse(toks, pos)) {
            items.push_back(std::move(*item));
            ++count;
            continue;
          }

          pos = saved;
          LOG_ERROR("Failed parsing item in module '" + name + "'");
          return std::nullopt;
        }
      });
}

inline parsec::Parser<std::shared_ptr<EnumDecl>> Parser::parse_enum() {
  using EV = EnumDecl::EnumVariant;

  auto variant = parsec::identifier.map([](const std::string &name) {
    EV v;
    v.name = name;
    return v;
  });

  return parsec::Parser<std::shared_ptr<EnumDecl>>(
      [variant](const std::vector<rc::Token> &toks,
                size_t &pos) -> parsec::ParseResult<std::shared_ptr<EnumDecl>> {
        size_t saved = pos;
        if (!tok(TokenType::ENUM).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        auto maybe_name = parsec::identifier.parse(toks, pos);
        if (!maybe_name) {
          pos = saved;
          return std::nullopt;
        }
        std::string name = *maybe_name;

        if (!tok(TokenType::L_BRACE).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        std::vector<EV> variants;

        // empty enum
        {
          size_t before = pos;
          if (tok(TokenType::R_BRACE).parse(toks, pos)) {
            LOG_DEBUG("Parsed empty enum: " + name);
            return std::make_shared<EnumDecl>(name, std::move(variants));
          }
          pos = before;
        }

        auto first = variant.parse(toks, pos);
        if (!first) {
          pos = saved;
          LOG_ERROR("Expected enum variant after '{' in enum '" + name + "'");
          return std::nullopt;
        }
        variants.push_back(*first);

        for (;;) {
          size_t before = pos;
          if (!tok(TokenType::COMMA).parse(toks, pos)) {
            pos = before;
            break;
          }
          // trailing comma
          size_t before_next = pos;
          if (tok(TokenType::R_BRACE).parse(toks, pos)) {
            LOG_DEBUG("Parsed enum '" + name + "' with " +
                      std::to_string(variants.size()) +
                      " variants (trailing comma)");
            return std::make_shared<EnumDecl>(name, std::move(variants));
          }
          pos = before_next;

          auto next = variant.parse(toks, pos);
          if (!next) {
            pos = saved;
            LOG_ERROR("Expected enum variant after ',' in enum '" + name + "'");
            return std::nullopt;
          }
          variants.push_back(*next);
        }

        if (!tok(TokenType::R_BRACE).parse(toks, pos)) {
          pos = saved;
          LOG_ERROR("Expected '}' to close enum '" + name + "'");
          return std::nullopt;
        }

        LOG_DEBUG("Parsed enum '" + name + "' with " +
                  std::to_string(variants.size()) + " variants");
        return std::make_shared<EnumDecl>(name, std::move(variants));
      });
}

inline parsec::Parser<std::shared_ptr<ConstantItem>>
Parser::parse_const_item() {
  // const name : Type ( = expr )? ;
  auto header = tok(TokenType::CONST)
                    .thenR(parsec::identifier)
                    .thenL(tok(TokenType::COLON))
                    .combine(typ, [](const auto &name, const auto &ty) {
                      return std::make_pair(name, ty);
                    });

  auto init_opt = optional(tok(TokenType::ASSIGN).thenR(any_expression()));

  return header
      .combine(init_opt,
               [](auto h, auto init) {
                 return std::make_tuple(h.first, h.second, init);
               })
      .thenL(tok(TokenType::SEMICOLON))
      .map([](auto t) {
        const std::string &name = std::get<0>(t);
        const LiteralType &ty = std::get<1>(t);
        const auto &init = std::get<2>(t);
        LOG_DEBUG("Parsed const item: " + name);
        return std::make_shared<ConstantItem>(name, ty, init);
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_return_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        LOG_DEBUG("Attempting to parse return expression at position " +
                  std::to_string(pos));
        size_t saved = pos;

        auto ret_parse =
            tok(TokenType::RETURN)
                .thenR(optional(any_expression()))
                .map([](auto val) {
                  LOG_DEBUG("Parsed return expression");
                  return std::make_shared<ReturnExpression>(std::move(val));
                });

        auto result = ret_parse.parse(toks, pos);
        if (!result) {
          LOG_ERROR("Failed to parse return expression at position " +
                    std::to_string(pos));
          pos = saved;
          return std::nullopt;
        }

        LOG_DEBUG("Successfully parsed return expression");
        return *result;
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_block_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse block expression at position " +
                  std::to_string(pos));
        if (!tok(TokenType::L_BRACE).parse(toks, pos)) {
          pos = saved;
          LOG_DEBUG("No opening brace found for block expression");
          return std::nullopt;
        }

        std::vector<std::shared_ptr<BaseNode>> stmts;
        std::optional<std::shared_ptr<Expression>> tail_expr;

        auto expr = any_expression(); // TODO, fix should not be any_expression,
                                      // should be `ExprWithoutBlock`
        auto empty_stmt = tok(TokenType::SEMICOLON).map([](auto) {
          return std::shared_ptr<BaseNode>(std::make_shared<EmptyStatement>());
        });
        auto let_stmt = parse_let_statement();
        auto expr_stmt = expr.thenL(tok(TokenType::SEMICOLON)).map([](auto e) {
          return std::shared_ptr<BaseNode>(
              std::make_shared<ExpressionStatement>(e, true));
        });
        auto item = any_top_level_item();

        int stmt_count = 0;

        for (;;) {
          size_t before = pos;
          if (auto s = let_stmt.parse(toks, pos)) {
            stmts.push_back(*s);
            stmt_count++;
            LOG_DEBUG("Parsed statement #" + std::to_string(stmt_count) +
                      " in block (let)");
            continue;
          }
          pos = before;
          if (auto s = empty_stmt.parse(toks, pos)) {
            stmts.push_back(*s);
            stmt_count++;
            LOG_DEBUG("Parsed statement #" + std::to_string(stmt_count) +
                      " in block (empty)");
            continue;
          }
          pos = before;
          if (auto s = expr_stmt.parse(toks, pos)) {
            stmts.push_back(*s);
            stmt_count++;
            LOG_DEBUG("Parsed statement #" + std::to_string(stmt_count) +
                      " in block (expr)");
            continue;
          }
          pos = before;
          if (auto s = item.parse(toks, pos)) {
            stmts.push_back(*s);
            stmt_count++;
            LOG_DEBUG("Parsed statement #" + std::to_string(stmt_count) +
                      " in block (item)");
            continue;
          }
          pos = before;
          break; // no more statements
        }

        size_t before_tail = pos;
        if (auto e = expr.parse(toks, pos)) {
          tail_expr = *e;
          LOG_DEBUG("Block has tail expression");
        } else {
          pos = before_tail;
          LOG_DEBUG("Block has no tail expression");
        }

        if (!tok(TokenType::R_BRACE).parse(toks, pos)) {
          pos = saved;
          LOG_ERROR("Missing closing brace for block expression");
          return std::nullopt;
        }

        LOG_DEBUG("Successfully parsed block expression with " +
                  std::to_string(stmt_count) + " statements" +
                  (tail_expr ? " and tail expression" : ""));
        return std::shared_ptr<Expression>(
            std::make_shared<BlockExpression>(std::move(stmts), tail_expr));
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_break_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        LOG_DEBUG("Attempting to parse break expression at position " +
                  std::to_string(pos));
        size_t saved = pos;

        auto break_parser = tok(TokenType::BREAK)
                                .thenR(optional(any_expression()))
                                .map([](auto t) {
                                  LOG_DEBUG("Parsed break expression");
                                  return std::make_shared<BreakExpression>(t);
                                });

        if (auto result = break_parser.parse(toks, pos)) {
          LOG_DEBUG("Successfully parsed break expression");
          return *result;
        }

        pos = saved;
        LOG_ERROR("Failed to parse break expression at position " +
                  std::to_string(pos));
        return std::nullopt;
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_continue_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [](const std::vector<rc::Token> &toks,
         size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        LOG_DEBUG("Attempting to parse continue expression at position " +
                  std::to_string(pos));
        size_t saved = pos;

        auto continue_parser = tok(TokenType::CONTINUE).map([](auto) {
          LOG_DEBUG("Parsed continue expression");
          return std::make_shared<ContinueExpression>();
        });

        if (auto result = continue_parser.parse(toks, pos)) {
          LOG_DEBUG("Successfully parsed continue expression");
          return *result;
        }

        pos = saved;
        LOG_ERROR("Failed to parse continue expression at position " +
                  std::to_string(pos));
        return std::nullopt;
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_if_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse if expression at position " +
                  std::to_string(pos));
        if (!tok(TokenType::IF).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        // Condition uses Pratt expressions
        LOG_DEBUG("Parsing if condition");
        auto cond = any_expression().parse(toks, pos);
        if (!cond) {
          pos = saved;
          LOG_ERROR("Failed to parse if condition");
          return std::nullopt;
        }

        // Then is a block expression
        LOG_DEBUG("Parsing if then block");
        auto then_blk = parse_block_expression().parse(toks, pos);
        if (!then_blk) {
          pos = saved;
          LOG_ERROR("Failed to parse if then block");
          return std::nullopt;
        }

        std::optional<std::shared_ptr<Expression>> else_expr;
        size_t before_else = pos;
        if (tok(TokenType::ELSE).parse(toks, pos)) {
          LOG_DEBUG("Parsing else clause");
          size_t after_else = pos;
          if (auto eb = parse_block_expression().parse(toks, pos)) {
            else_expr = *eb;
            LOG_DEBUG("Parsed else block");
          } else {
            pos = after_else;
            if (auto ei = parse_if_expression().parse(toks, pos)) {
              else_expr = *ei;
              LOG_DEBUG("Parsed else if");
            } else {
              pos = saved;
              LOG_ERROR("Failed to parse else clause");
              return std::nullopt;
            }
          }
        } else {
          pos = before_else;
          LOG_DEBUG("No else clause found");
        }

        LOG_DEBUG("Successfully parsed if expression");
        return std::shared_ptr<Expression>(
            std::make_shared<IfExpression>(*cond, *then_blk, else_expr));
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_loop_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse loop expression at position " +
                  std::to_string(pos));

        auto loop_parser = tok(TokenType::LOOP).thenR(parse_block_expression());

        auto body = loop_parser.parse(toks, pos);
        if (!body) {
          pos = saved;
          LOG_ERROR("Failed to parse loop body");
          return std::nullopt;
        }

        LOG_DEBUG("Successfully parsed loop expression");
        return std::shared_ptr<Expression>(
            std::make_shared<LoopExpression>(*body));
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_while_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse while expression at position " +
                  std::to_string(pos));

        auto while_parser =
            tok(TokenType::WHILE)
                .thenR(any_expression())
                .combine(parse_block_expression(),
                         [](const auto &cond, const auto &body) {
                           return std::make_shared<WhileExpression>(cond, body);
                         });

        auto result = while_parser.parse(toks, pos);
        if (!result) {
          pos = saved;
          LOG_ERROR("Failed to parse while expression at position " +
                    std::to_string(pos));
          return std::nullopt;
        }

        LOG_DEBUG("Successfully parsed while expression");
        return *result;
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_match_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse match expression at position " +
                  std::to_string(pos));
        if (!tok(TokenType::MATCH).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        auto scrutinee = any_expression().parse(toks, pos);
        if (!scrutinee) {
          pos = saved;
          LOG_ERROR("Failed to parse match scrutinee");
          return std::nullopt;
        }

        if (!tok(TokenType::L_BRACE).parse(toks, pos)) {
          pos = saved;
          LOG_ERROR("Expected '{' after match scrutinee");
          return std::nullopt;
        }

        std::vector<MatchExpression::MatchArm> arms;

        // TODO: Implement MatchArms parsing

        return std::make_shared<MatchExpression>(*scrutinee, std::move(arms));
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_array_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        if (!tok(TokenType::L_BRACKET).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        // [] empty array
        if (tok(TokenType::R_BRACKET).parse(toks, pos)) {
          LOG_DEBUG("Parsed empty array literal");
          return std::make_shared<ArrayExpression>(
              std::vector<std::shared_ptr<Expression>>{});
        }

        auto first = any_expression().parse(toks, pos);
        if (!first) {
          pos = saved;
          LOG_ERROR("Expected expression in array literal");
          return std::nullopt;
        }

        // repeat: expr ; expr ]
        size_t after_first = pos;
        if (tok(TokenType::SEMICOLON).parse(toks, pos)) {
          auto count = any_expression().parse(toks, pos);
          if (!count) {
            pos = saved;
            LOG_ERROR(
                "Expected size expression after ';' in array repeat literal");
            return std::nullopt;
          }
          if (!tok(TokenType::R_BRACKET).parse(toks, pos)) {
            pos = saved;
            LOG_ERROR("Expected ']' to close array repeat literal");
            return std::nullopt;
          }
          LOG_DEBUG("Parsed repeat array literal");
          return std::make_shared<ArrayExpression>(*first, *count);
        }
        pos = after_first;

        // elements list: e1 (, eN)* ,? ]
        std::vector<std::shared_ptr<Expression>> elems;
        elems.push_back(*first);
        for (;;) {
          size_t before = pos;
          if (!tok(TokenType::COMMA).parse(toks, pos)) {
            pos = before;
            break;
          }
          // trailing comma
          size_t before_elem = pos;
          if (tok(TokenType::R_BRACKET).parse(toks, pos)) {
            LOG_DEBUG("Parsed array literal with trailing comma");
            return std::make_shared<ArrayExpression>(std::move(elems));
          }
          pos = before_elem;
          auto next = any_expression().parse(toks, pos);
          if (!next) {
            pos = saved;
            LOG_ERROR("Expected expression after ',' in array literal");
            return std::nullopt;
          }
          elems.push_back(*next);
        }

        if (!tok(TokenType::R_BRACKET).parse(toks, pos)) {
          pos = saved;
          LOG_ERROR("Expected ']' to close array literal");
          return std::nullopt;
        }

        LOG_DEBUG("Parsed array literal with " + std::to_string(elems.size()) +
                  " elements");
        return std::make_shared<ArrayExpression>(std::move(elems));
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_tuple_or_group_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        if (!tok(TokenType::L_PAREN).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        if (tok(TokenType::R_PAREN).parse(toks, pos)) {
          LOG_DEBUG("Parsed unit tuple expression");
          return std::make_shared<TupleExpression>(
              std::vector<std::shared_ptr<Expression>>{});
        }

        auto first = any_expression().parse(toks, pos);
        if (!first) {
          pos = saved;
          LOG_ERROR("Expected expression inside parentheses");
          return std::nullopt;
        }

        // it's a tuple, but no tuple expr anymore
        if (tok(TokenType::COMMA).parse(toks, pos)) {
          std::vector<std::shared_ptr<Expression>> elems;
          elems.push_back(*first);

          for (;;) {
            size_t before = pos;
            if (tok(TokenType::R_PAREN).parse(toks, pos)) {
              LOG_DEBUG("Parsed tuple expression with trailing comma");
              return std::make_shared<TupleExpression>(std::move(elems));
            }
            pos = before;

            auto next = any_expression().parse(toks, pos);
            if (!next) {
              pos = saved;
              LOG_ERROR("Expected expression after ',' in tuple");
              return std::nullopt;
            }
            elems.push_back(*next);

            if (!tok(TokenType::COMMA).parse(toks, pos)) {
              break;
            }
          }

          if (!tok(TokenType::R_PAREN).parse(toks, pos)) {
            pos = saved;
            LOG_ERROR("Expected ')' to close tuple expression");
            return std::nullopt;
          }

          LOG_DEBUG("Parsed tuple expression with " +
                    std::to_string(elems.size()) + " elements");
          return std::make_shared<TupleExpression>(std::move(elems));
        }

        // group expression
        if (!tok(TokenType::R_PAREN).parse(toks, pos)) {
          pos = saved;
          LOG_ERROR("Expected ')' to close parenthesized expression");
          return std::nullopt;
        }

        LOG_DEBUG("Parsed parenthesized expression (group)");
        return std::make_shared<GroupExpression>(*first);
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_path_or_name_expression() {
  auto path_ident = parsec::Parser<std::pair<std::string, bool>>(
      [](const std::vector<rc::Token> &toks,
         size_t &pos) -> parsec::ParseResult<std::pair<std::string, bool>> {
        size_t saved = pos;
        if (auto id = parsec::identifier.parse(toks, pos)) {
          return std::make_pair(*id, true);
        }
        pos = saved;
        if (auto t = tok(TokenType::SELF).parse(toks, pos))
          return std::make_pair(std::string("self"), false);
        pos = saved;
        if (auto t = tok(TokenType::SELF_TYPE).parse(toks, pos))
          return std::make_pair(std::string("Self"), false);
        pos = saved;
        if (auto t = tok(TokenType::SUPER).parse(toks, pos))
          return std::make_pair(std::string("super"), false);
        pos = saved;
        if (auto t = tok(TokenType::CRATE).parse(toks, pos))
          return std::make_pair(std::string("crate"), false);
        pos = saved;
        return std::nullopt;
      });

  return parsec::Parser<std::shared_ptr<
      Expression>>([path_ident](const std::vector<rc::Token> &toks, size_t &pos)
                       -> parsec::ParseResult<std::shared_ptr<Expression>> {
    size_t saved = pos;

    // ::? PathExprSegment ( :: PathExprSegment )*
    bool leading = false;
    {
      size_t before = pos;
      if (tok(TokenType::COLON_COLON).parse(toks, pos)) {
        leading = true;
      } else {
        pos = before;
      }
    }

    auto first = path_ident.parse(toks, pos);
    if (!first) {
      pos = saved;
      return std::nullopt;
    }

    std::vector<rc::PathExpression::Segment> segs;
    segs.push_back(rc::PathExpression::Segment{first->first, std::nullopt});

    bool saw_colon = false;
    for (;;) {
      size_t before = pos;
      if (!tok(TokenType::COLON_COLON).parse(toks, pos)) {
        pos = before;
        break;
      }
      saw_colon = true;
      auto next = path_ident.parse(toks, pos);
      if (!next) {
        pos = saved;
        return std::nullopt;
      }
      segs.push_back(rc::PathExpression::Segment{next->first, std::nullopt});
    }

    // it is an identifier
    if (!leading && !saw_colon && first->second) {
      return std::make_shared<rc::NameExpression>(first->first);
    }

    return std::make_shared<rc::PathExpression>(leading, std::move(segs));
  });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_qualified_path_expression() {
  auto path_ident = parsec::Parser<std::string>(
      [](const std::vector<rc::Token> &toks,
         size_t &pos) -> parsec::ParseResult<std::string> {
        size_t saved = pos;
        if (auto id = parsec::identifier.parse(toks, pos))
          return *id;
        pos = saved;
        if (tok(TokenType::SELF).parse(toks, pos))
          return std::string("self");
        pos = saved;
        if (tok(TokenType::SELF_TYPE).parse(toks, pos))
          return std::string("Self");
        pos = saved;
        if (tok(TokenType::SUPER).parse(toks, pos))
          return std::string("super");
        pos = saved;
        if (tok(TokenType::CRATE).parse(toks, pos))
          return std::string("crate");
        pos = saved;
        return std::nullopt;
      });

  return parsec::Parser<std::shared_ptr<Expression>>(
      [path_ident](const std::vector<rc::Token> &toks, size_t &pos)
          -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        if (!tok(TokenType::LT).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        auto base_ty = typ.parse(toks, pos);
        if (!base_ty) {
          pos = saved;
          return std::nullopt;
        }

        // TODO: fix Type Path here
        std::optional<std::vector<std::string>> as_path;
        {
          size_t before_as = pos;
          if (tok(TokenType::AS).parse(toks, pos)) {
            {
              size_t before = pos;
              if (!tok(TokenType::COLON_COLON).parse(toks, pos))
                pos = before;
            }
            auto first = path_ident.parse(toks, pos);
            if (!first) {
              pos = saved;
              return std::nullopt;
            }
            std::vector<std::string> tpath;
            tpath.push_back(*first);

            for (;;) {
              size_t b = pos;
              if (!tok(TokenType::COLON_COLON).parse(toks, pos)) {
                pos = b;
                break;
              }
              auto seg = path_ident.parse(toks, pos);
              if (!seg) {
                pos = saved;
                return std::nullopt;
              }
              tpath.push_back(*seg);
            }
            as_path = std::move(tpath);
          } else {
            pos = before_as;
          }
        }

        if (!tok(TokenType::GT).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        if (!tok(TokenType::COLON_COLON).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }

        std::vector<rc::PathExpression::Segment> segs;
        auto first_seg = path_ident.parse(toks, pos);
        if (!first_seg) {
          pos = saved;
          return std::nullopt;
        }
        segs.push_back(rc::PathExpression::Segment{*first_seg, std::nullopt});
        for (;;) {
          size_t before = pos;
          if (!tok(TokenType::COLON_COLON).parse(toks, pos)) {
            pos = before;
            break;
          }
          auto nxt = path_ident.parse(toks, pos);
          if (!nxt) {
            pos = saved;
            return std::nullopt;
          }
          segs.push_back(rc::PathExpression::Segment{*nxt, std::nullopt});
        }

        if (segs.empty()) {
          pos = saved;
          return std::nullopt;
        }

        return std::make_shared<rc::QualifiedPathExpression>(
            *base_ty, std::move(as_path), std::move(segs));
      });
}

inline parsec::Parser<std::shared_ptr<Expression>> Parser::any_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse any expression via Pratt at position " +
                  std::to_string(pos));

        // Use the PrattTable member of our class to parse.
        if (auto e = pratt_table_.parse_expression(toks, pos)) {
          LOG_DEBUG("Pratt parser succeeded.");
          return e;
        }

        pos = saved;
        LOG_DEBUG("Pratt parser failed to parse an expression.");
        return std::nullopt;
      });
}

inline parsec::Parser<std::pair<std::string, LiteralType>>
Parser::identifier_and_type_parser() {
  using namespace parsec;
  return parsec::identifier.combine(
      tok(TokenType::COLON).thenR(typ), [](const auto &id, const auto &t) {
        const auto &ty = t;
        LOG_DEBUG("Parsed identifier with type: " + id);
        return std::make_pair(id, ty);
      });
}

inline parsec::Parser<std::vector<std::pair<std::string, LiteralType>>>
Parser::argument_list_parser() {
  using namespace parsec;
  auto identifier_and_type = identifier_and_type_parser();

  return tok(TokenType::L_PAREN)
      .thenR(many(identifier_and_type.thenL(optional(tok(TokenType::COMMA)))))
      .thenL(tok(TokenType::R_PAREN))
      .map([](auto args) {
        LOG_DEBUG("Parsed argument list with " + std::to_string(args.size()) +
                  " arguments");
        return args;
      });
}

inline parsec::Parser<std::vector<std::shared_ptr<Expression>>>
Parser::expression_list_parser() {
  using namespace parsec;
  auto expr = any_expression();

  return tok(TokenType::L_PAREN)
      .thenR(many(expr.thenL(optional(tok(TokenType::COMMA)))))
      .thenL(tok(TokenType::R_PAREN))
      .map([](auto exprs) {
        LOG_DEBUG("Parsed expression list with " +
                  std::to_string(exprs.size()) + " expressions");
        return exprs;
      });
}

inline parsec::Parser<std::shared_ptr<BaseNode>> Parser::parse_let_statement() {
  using namespace parsec;
  auto assignment = tok(TokenType::ASSIGN).thenR(any_expression());
  return tok(TokenType::LET)
      .thenR(pattern_parser_.pattern_no_top_alt())
      .combine(tok(TokenType::COLON).thenR(typ),
               [](const auto &id, const auto &t) {
                 auto ty = t;
                 return std::make_pair(id, ty);
               })
      .combine(optional(assignment),
               [](const auto &id_ty, const auto &init) {
                 return std::make_tuple(id_ty.first, id_ty.second, init);
               })
      .thenL(tok(TokenType::SEMICOLON))
      .map([](auto t) -> std::shared_ptr<BaseNode> {
        const auto &pattern = std::get<0>(t);
        const auto &type = std::get<1>(t);
        const auto &init = std::get<2>(t).value_or(nullptr);
        LOG_DEBUG("Parsed let statement");
        return std::make_shared<LetStatement>(pattern, type, init);
      });
}

inline parsec::Parser<std::vector<rc::StructExpression::FieldInit>>
Parser::parse_struct_expr_fields() {
  using Field = rc::StructExpression::FieldInit;
  using namespace parsec;

  auto shorthand = parsec::identifier.map([](const std::string &name) {
    Field f{name, std::nullopt};
    return f;
  });

  auto explicit_field =
      parsec::identifier.thenL(tok(TokenType::COLON))
          .combine(any_expression(), [](const std::string &name,
                                        std::shared_ptr<Expression> expr) {
            Field f{name, expr};
            return f;
          });

  auto one_field = parsec::Parser<Field>(
      [shorthand, explicit_field](const std::vector<rc::Token> &toks,
                                  size_t &pos) -> parsec::ParseResult<Field> {
        size_t saved = pos;
        if (auto f = explicit_field.parse(toks, pos))
          return *f;
        pos = saved;
        if (auto f = shorthand.parse(toks, pos))
          return *f;
        return std::nullopt;
      });

  return tok(TokenType::L_BRACE)
      .thenR(many(one_field.thenL(optional(tok(TokenType::COMMA)))))
      .thenL(tok(TokenType::R_BRACE));
}

} // namespace rc

namespace pratt {
inline PrattTable default_table(rc::Parser *p) {
  PrattTable tbl;

  auto delegate_to_parsec =
      [p](parsec::Parser<ExprPtr> (rc::Parser::*parser_method)()) {
        return [p, parser_method](const std::vector<rc::Token> &toks,
                                  size_t &pos) -> ExprPtr {
          size_t start_pos = pos - 1;
          if (auto result = (p->*parser_method)().parse(toks, start_pos)) {
            pos = start_pos;
            return *result;
          }
          return nullptr;
        };
      };

  tbl.prefix(rc::TokenType::IF,
             delegate_to_parsec(&rc::Parser::parse_if_expression));
  tbl.prefix(rc::TokenType::LOOP,
             delegate_to_parsec(&rc::Parser::parse_loop_expression));
  tbl.prefix(rc::TokenType::WHILE,
             delegate_to_parsec(&rc::Parser::parse_while_expression));
  tbl.prefix(rc::TokenType::L_BRACE,
             delegate_to_parsec(&rc::Parser::parse_block_expression));
  tbl.prefix(rc::TokenType::RETURN,
             delegate_to_parsec(&rc::Parser::parse_return_expression));
  tbl.prefix(rc::TokenType::MATCH,
             delegate_to_parsec(&rc::Parser::parse_match_expression));
  tbl.prefix(rc::TokenType::L_BRACKET,
             delegate_to_parsec(&rc::Parser::parse_array_expression));
  tbl.prefix(rc::TokenType::BREAK,
             delegate_to_parsec(&rc::Parser::parse_break_expression));
  tbl.prefix(rc::TokenType::CONTINUE,
             delegate_to_parsec(&rc::Parser::parse_continue_expression));

  // Literals
  auto add_simple_literal = [&tbl](rc::TokenType tt,
                                   rc::PrimitiveLiteralType plt) {
    tbl.prefix(
        tt, [plt](const std::vector<rc::Token> &toks, size_t &pos) -> ExprPtr {
          const rc::Token &prev = toks[pos - 1];
          return std::make_shared<rc::LiteralExpression>(prev.lexeme,
                                                         rc::LiteralType(plt));
        });
  };

  add_simple_literal(rc::TokenType::INTEGER_LITERAL,
                     rc::PrimitiveLiteralType::I32);
  add_simple_literal(rc::TokenType::STRING_LITERAL,
                     rc::PrimitiveLiteralType::STRING);
  add_simple_literal(rc::TokenType::C_STRING_LITERAL,
                     rc::PrimitiveLiteralType::C_STRING);
  add_simple_literal(rc::TokenType::CHAR_LITERAL,
                     rc::PrimitiveLiteralType::CHAR);
  add_simple_literal(rc::TokenType::TRUE, rc::PrimitiveLiteralType::BOOL);
  add_simple_literal(rc::TokenType::FALSE, rc::PrimitiveLiteralType::BOOL);

  // Path or Name expressions
  tbl.prefix(rc::TokenType::NON_KEYWORD_IDENTIFIER,
             delegate_to_parsec(&rc::Parser::parse_path_or_name_expression));
  tbl.prefix(rc::TokenType::SELF,
             delegate_to_parsec(&rc::Parser::parse_path_or_name_expression));
  tbl.prefix(rc::TokenType::SELF_TYPE,
             delegate_to_parsec(&rc::Parser::parse_path_or_name_expression));
  tbl.prefix(rc::TokenType::SUPER,
             delegate_to_parsec(&rc::Parser::parse_path_or_name_expression));
  tbl.prefix(rc::TokenType::CRATE,
             delegate_to_parsec(&rc::Parser::parse_path_or_name_expression));
  tbl.prefix(rc::TokenType::COLON_COLON,
             delegate_to_parsec(&rc::Parser::parse_path_or_name_expression));
  // Qualified path expression starting with '<'
  tbl.prefix(rc::TokenType::LT,
             delegate_to_parsec(&rc::Parser::parse_qualified_path_expression));

  tbl.prefix(rc::TokenType::L_PAREN,
             delegate_to_parsec(&rc::Parser::parse_tuple_or_group_expression));

  auto prefix_op = [&tbl](const std::vector<rc::Token> &toks,
                          size_t &pos) -> ExprPtr {
    rc::Token op = toks[pos - 1];
    ExprPtr right = tbl.parse_expression(toks, pos, 100); // Unary precedence
    if (!right)
      return nullptr;
    return std::make_shared<rc::PrefixExpression>(op, std::move(right));
  };
  tbl.prefix(rc::TokenType::PLUS, prefix_op);
  tbl.prefix(rc::TokenType::MINUS, prefix_op);
  tbl.prefix(rc::TokenType::NOT, prefix_op);

  auto bin = [](ExprPtr l, rc::Token op, ExprPtr r) {
    return std::make_shared<rc::BinaryExpression>(std::move(l), std::move(op),
                                                  std::move(r));
  };

  const int POSTFIX_PRECEDENCE = 90;

  // Expression list
  tbl.infix_custom(
      rc::TokenType::L_PAREN, POSTFIX_PRECEDENCE, POSTFIX_PRECEDENCE + 1,
      [p](ExprPtr left, const rc::Token &, const std::vector<rc::Token> &toks,
          size_t &pos) -> ExprPtr {
        size_t start_pos = pos - 1;
        auto args_result = p->expression_list_parser().parse(toks, start_pos);
        if (!args_result) {
          return nullptr;
        }
        pos = start_pos;
        return std::make_shared<rc::CallExpression>(std::move(left),
                                                    *args_result);
      });

  // Field access and Method call
  tbl.infix_custom(
      rc::TokenType::DOT, POSTFIX_PRECEDENCE, POSTFIX_PRECEDENCE + 1,
      [p](ExprPtr left, const rc::Token &, const std::vector<rc::Token> &toks,
          size_t &pos) -> ExprPtr {
        if (pos >= toks.size()) {
          return nullptr;
        }

        const rc::Token &field_token = toks[pos];

        // tuple field access
        if (field_token.type == rc::TokenType::INTEGER_LITERAL) {
          pos++;
          return std::make_shared<rc::FieldAccessExpression>(
              left, field_token.lexeme);
        }

        // struct field access or method calls
        if (field_token.type == rc::TokenType::NON_KEYWORD_IDENTIFIER) {
          pos++;
          if (pos < toks.size() && toks[pos].type == rc::TokenType::L_PAREN) {
            size_t start_pos = pos;
            auto args_result =
                p->expression_list_parser().parse(toks, start_pos);
            if (!args_result) {
              return nullptr;
            }
            pos = start_pos;
            auto segment = rc::MethodCallExpression::PathExprSegment{
                field_token.lexeme, std::nullopt};
            return std::make_shared<rc::MethodCallExpression>(left, segment,
                                                              *args_result);
          } else {
            return std::make_shared<rc::FieldAccessExpression>(
                left, field_token.lexeme);
          }
        }

        return nullptr;
      });

  // Struct expression: PathInExpression { fields }
  tbl.infix_custom(
      rc::TokenType::L_BRACE, POSTFIX_PRECEDENCE, POSTFIX_PRECEDENCE + 1,
      [p](ExprPtr left, const rc::Token &, const std::vector<rc::Token> &toks,
          size_t &pos) -> ExprPtr {
        if (!left)
          return nullptr;
        if (!(dynamic_cast<rc::NameExpression *>(left.get()) ||
              dynamic_cast<rc::PathExpression *>(left.get()) ||
              dynamic_cast<rc::QualifiedPathExpression *>(left.get()))) {
          return nullptr;
        }
        size_t start_pos = pos - 1;
        auto fields = p->parse_struct_expr_fields().parse(toks, start_pos);
        if (!fields) {
          return nullptr;
        }
        pos = start_pos;
        return std::make_shared<rc::StructExpression>(left, *fields);
      });

  // 80: index expression
  tbl.infix_custom(
      rc::TokenType::L_BRACKET, 80, 81,
      [&tbl](ExprPtr left, const rc::Token &,
             const std::vector<rc::Token> &toks, size_t &pos) -> ExprPtr {
        ExprPtr index = tbl.parse_expression(toks, pos, 0);
        if (!index)
          return nullptr;
        if (pos >= toks.size() || toks[pos].type != rc::TokenType::R_BRACKET)
          return nullptr;
        ++pos; // consume ']'
        return std::make_shared<rc::IndexExpression>(left, index);
      });

  // 70: * / %
  tbl.infix_left(rc::TokenType::STAR, 70, bin);
  tbl.infix_left(rc::TokenType::SLASH, 70, bin);
  tbl.infix_left(rc::TokenType::PERCENT, 70, bin);
  // 60: + -
  tbl.infix_left(rc::TokenType::PLUS, 60, bin);
  tbl.infix_left(rc::TokenType::MINUS, 60, bin);
  // 50: << >>
  tbl.infix_left(rc::TokenType::SHL, 50, bin);
  tbl.infix_left(rc::TokenType::SHR, 50, bin);
  // 40: & (bitwise)
  tbl.infix_left(rc::TokenType::AMPERSAND, 40, bin);
  // 35: ^
  tbl.infix_left(rc::TokenType::CARET, 35, bin);
  // 30: |
  tbl.infix_left(rc::TokenType::PIPE, 30, bin);
  // 20: < > <= >=
  tbl.infix_left(rc::TokenType::LT, 20, bin);
  tbl.infix_left(rc::TokenType::GT, 20, bin);
  tbl.infix_left(rc::TokenType::LE, 20, bin);
  tbl.infix_left(rc::TokenType::GE, 20, bin);
  // 15: == !=
  tbl.infix_left(rc::TokenType::NE, 15, bin);
  tbl.infix_left(rc::TokenType::EQ, 15, bin);
  // 10: &&
  tbl.infix_left(rc::TokenType::AND, 10, bin);
  // 9: ||
  tbl.infix_left(rc::TokenType::OR, 9, bin);
  // 5: assignments
  tbl.infix_right(rc::TokenType::ASSIGN, 5, bin);
  tbl.infix_right(rc::TokenType::PLUS_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::MINUS_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::STAR_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::SLASH_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::PERCENT_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::AMPERSAND_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::PIPE_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::CARET_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::SHL_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::SHR_EQ, 5, bin);

  return tbl;
}

} // namespace pratt