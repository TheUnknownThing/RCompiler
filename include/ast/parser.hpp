#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "../lexer/lexer.hpp"
#include "../utils/logger.hpp"
#include "types.hpp"
#include "utils/parsec.hpp"
#include "utils/pratt.hpp"

#include "nodes/base.hpp"
#include "nodes/expr.hpp"
#include "nodes/stmt.hpp"
#include "nodes/topLevel.hpp"

using namespace parsec;

namespace rc {

class Parser {
public:
  Parser(std::vector<Token> tokens);
  std::unique_ptr<RootNode> parse();

private:
  std::vector<Token> tokens;

  std::unique_ptr<BaseNode> parse_item();

  std::unique_ptr<BaseNode> parse_statement();
  std::unique_ptr<BaseNode> parse_expression();

  parsec::Parser<std::unique_ptr<FunctionDecl>> parse_function();
  parsec::Parser<std::unique_ptr<StructDecl>> parse_struct();

  parsec::Parser<std::shared_ptr<Expression>> pratt_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_block_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_if_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_return_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_call_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_loop_expression();
  parsec::Parser<std::shared_ptr<Expression>> parse_while_expression();
  parsec::Parser<std::shared_ptr<Expression>> any_expression();
  parsec::Parser<std::shared_ptr<Expression>>
  parse_primary_literal_expression();

  parsec::Parser<std::pair<std::string, LiteralType>>
  identifier_and_type_parser();
  parsec::Parser<std::vector<std::pair<std::string, LiteralType>>>
  argument_list_parser();
  parsec::Parser<std::vector<std::shared_ptr<Expression>>>
  expression_list_parser();
};

inline Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {
  LOG_DEBUG("Parser initialized with " + std::to_string(this->tokens.size()) +
            " tokens");
}

inline std::unique_ptr<RootNode> Parser::parse() {
  LOG_INFO("Starting parsing process");
  auto root = std::make_unique<RootNode>();

  // top level item parser
  auto item_parser = parsec::Parser<std::unique_ptr<BaseNode>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::unique_ptr<BaseNode>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse function at position " +
                  std::to_string(pos));
        if (auto f = parse_function().parse(toks, pos)) {
          LOG_DEBUG("Successfully parsed function");
          return std::unique_ptr<BaseNode>(f->release());
        }
        pos = saved;
        LOG_DEBUG("Function parsing failed, attempting struct at position " +
                  std::to_string(pos));
        if (auto s = parse_struct().parse(toks, pos)) {
          LOG_DEBUG("Successfully parsed struct");
          return std::unique_ptr<BaseNode>(s->release());
        }
        pos = saved;
        LOG_DEBUG("No top-level item found at position " + std::to_string(pos));
        return std::nullopt;
      });

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

inline std::unique_ptr<BaseNode> Parser::parse_item() {
  LOG_DEBUG("Parsing single item");
  // One-shot item parser
  auto item_parser = parsec::Parser<std::unique_ptr<BaseNode>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::unique_ptr<BaseNode>> {
        size_t saved = pos;
        if (auto f = parse_function().parse(toks, pos)) {
          LOG_DEBUG("Single item parser: found function");
          return std::unique_ptr<BaseNode>(f->release());
        }
        pos = saved;
        if (auto s = parse_struct().parse(toks, pos)) {
          LOG_DEBUG("Single item parser: found struct");
          return std::unique_ptr<BaseNode>(s->release());
        }
        pos = saved;
        LOG_DEBUG("Single item parser: no item found");
        return std::nullopt;
      });
  size_t pos = 0;
  if (auto r = item_parser.parse(tokens, pos)) {
    LOG_DEBUG("Successfully parsed single item");
    return std::move(*r);
  }
  LOG_WARN("Failed to parse single item");
  return nullptr;
}

inline std::unique_ptr<BaseNode> Parser::parse_statement() {
  LOG_DEBUG("Parsing statement");
  auto expr = any_expression();

  auto let_stmt =
      tok(TokenType::LET)
          .thenR(parsec::identifier)
          .combine(optional(tok(TokenType::COLON).thenR(typ)),
                   [](const auto &id, const auto &t) {
                     auto ty = t.value_or(
                         LiteralType(PrimitiveLiteralType::TO_BE_INFERRED));
                     return std::make_pair(id, ty);
                   })
          .thenL(tok(TokenType::ASSIGN))
          .combine(expr,
                   [](const auto &id_ty, auto e) {
                     return std::tuple<std::string, LiteralType,
                                       std::shared_ptr<Expression>>{
                         id_ty.first, id_ty.second, std::move(e)};
                   })
          .thenL(tok(TokenType::SEMICOLON))
          .map([](auto t) -> std::unique_ptr<BaseNode> {
            LOG_DEBUG("Parsed let statement for variable: " + std::get<0>(t));
            return std::make_unique<LetStatement>(
                std::get<0>(t), std::get<1>(t), std::get<2>(t));
          });

  auto empty_stmt =
      tok(TokenType::SEMICOLON).map([](auto) -> std::unique_ptr<BaseNode> {
        LOG_DEBUG("Parsed empty statement");
        return std::make_unique<EmptyStatement>();
      });

  // expr ; or expr (no semicolon)
  auto expr_stmt = expr.combine(
      optional(tok(TokenType::SEMICOLON)), [](auto e, const auto &semi) {
        return std::unique_ptr<BaseNode>(
            new ExpressionStatement(e, semi.has_value()));
      });

  auto stmt = parsec::Parser<std::unique_ptr<BaseNode>>(
      [let_stmt, empty_stmt, expr_stmt](
          const std::vector<rc::Token> &toks,
          size_t &pos) -> parsec::ParseResult<std::unique_ptr<BaseNode>> {
        size_t saved = pos;
        if (auto r = let_stmt.parse(toks, pos))
          return std::move(*r);
        pos = saved;
        if (auto r = empty_stmt.parse(toks, pos))
          return std::move(*r);
        pos = saved;
        if (auto r = expr_stmt.parse(toks, pos))
          return std::move(*r);
        pos = saved;
        return std::nullopt;
      });

  size_t pos = 0;
  if (auto r = stmt.parse(tokens, pos)) {
    LOG_DEBUG("Successfully parsed statement");
    return std::move(*r);
  }
  LOG_WARN("Failed to parse statement");
  return nullptr;
}

inline std::unique_ptr<BaseNode> Parser::parse_expression() {
  LOG_DEBUG("Expression parsing not implemented yet");
  // TODO
  return nullptr;
}

inline parsec::Parser<std::unique_ptr<FunctionDecl>> Parser::parse_function() {
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
            auto ret_ty =
                ty.value_or(LiteralType(PrimitiveLiteralType::TO_BE_INFERRED));
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
    return std::make_unique<FunctionDecl>(std::get<0>(h), std::get<1>(h),
                                          std::get<2>(h), b);
  });
}

inline parsec::Parser<std::unique_ptr<StructDecl>> Parser::parse_struct() {
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
                [&](auto &&val) -> std::unique_ptr<StructDecl> {
                  using T = std::decay_t<decltype(val)>;
                  if constexpr (std::is_same_v<
                                    T, std::vector<std::pair<std::string,
                                                             LiteralType>>>) {
                    LOG_DEBUG("Successfully parsed named struct: " + name +
                              " with " + std::to_string(val.size()) +
                              " fields");
                    return std::make_unique<StructDecl>(
                        name, SD::StructType::Struct, val,
                        std::vector<LiteralType>{});
                  } else {
                    LOG_DEBUG("Successfully parsed tuple struct: " + name +
                              " with " + std::to_string(val.size()) +
                              " fields");
                    return std::make_unique<StructDecl>(
                        name, SD::StructType::Tuple,
                        std::vector<std::pair<std::string, LiteralType>>{},
                        val);
                  }
                },
                var);
          });

  return parser;
}

inline parsec::Parser<std::shared_ptr<Expression>> Parser::pratt_expression() {
  LOG_DEBUG("Parsing Pratt expression");
  auto tbl = pratt::default_table();
  return pratt::pratt_expr(tbl);
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
Parser::parse_call_expression() {
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        LOG_DEBUG("Attempting to parse function call at position " +
                  std::to_string(pos));
        size_t saved = pos;

        auto func_parse = identifier.combine(
            expression_list_parser(), [](const auto &name, const auto &args) {
              LOG_DEBUG("Parsed function call: " + name + " with " +
                        std::to_string(args.size()) + " arguments");
              return std::make_shared<CallExpression>(name, args);
            });

        auto result = func_parse.parse(toks, pos);
        if (!result) {
          LOG_ERROR("Failed to parse function call at position " +
                    std::to_string(pos));
          pos = saved;
          return std::nullopt;
        }

        LOG_DEBUG("Successfully parsed function call");
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

        auto expr = any_expression();
        auto empty_stmt = tok(TokenType::SEMICOLON).map([](auto) {
          return std::shared_ptr<BaseNode>(std::make_shared<EmptyStatement>());
        });
        auto let_stmt =
            tok(TokenType::LET)
                .thenR(parsec::identifier)
                .combine(optional(tok(TokenType::COLON).thenR(typ)),
                         [](const auto &id, const auto &t) {
                           auto ty = t.value_or(LiteralType(
                               PrimitiveLiteralType::TO_BE_INFERRED));
                           return std::make_pair(id, ty);
                         })
                .thenL(tok(TokenType::ASSIGN))
                .combine(expr,
                         [](const auto &id_ty, auto e) {
                           return std::tuple<std::string, LiteralType,
                                             std::shared_ptr<Expression>>{
                               id_ty.first, id_ty.second, std::move(e)};
                         })
                .thenL(tok(TokenType::SEMICOLON))
                .map([](auto t) {
                  return std::shared_ptr<BaseNode>(
                      std::make_shared<LetStatement>(
                          std::get<0>(t), std::get<1>(t), std::get<2>(t)));
                });
        auto expr_stmt = expr.thenL(tok(TokenType::SEMICOLON)).map([](auto e) {
          return std::shared_ptr<BaseNode>(
              std::make_shared<ExpressionStatement>(e, true));
        });

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
        auto cond = pratt_expression().parse(toks, pos);
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
        if (!tok(TokenType::LOOP).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }
        auto body = parse_block_expression().parse(toks, pos);
        if (!body) {
          pos = saved;
          LOG_ERROR("Failed to parse loop body block");
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
        if (!tok(TokenType::WHILE).parse(toks, pos)) {
          pos = saved;
          return std::nullopt;
        }
        LOG_DEBUG("Parsing while condition");
        auto cond = pratt_expression().parse(toks, pos);
        if (!cond) {
          pos = saved;
          LOG_ERROR("Failed to parse while condition");
          return std::nullopt;
        }
        LOG_DEBUG("Parsing while body block");
        auto body = parse_block_expression().parse(toks, pos);
        if (!body) {
          pos = saved;
          LOG_ERROR("Failed to parse while body block");
          return std::nullopt;
        }
        LOG_DEBUG("Successfully parsed while expression");
        return std::shared_ptr<Expression>(
            std::make_shared<WhileExpression>(*cond, *body));
      });
}

inline parsec::Parser<std::shared_ptr<Expression>> Parser::any_expression() {
  // try parse it sequentially
  return parsec::Parser<std::shared_ptr<Expression>>(
      [this](const std::vector<rc::Token> &toks,
             size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse any expression at position " +
                  std::to_string(pos));
        if (auto e = parse_if_expression().parse(toks, pos)) {
          LOG_DEBUG("Parsed if expression");
          return *e;
        }
        pos = saved;
        if (auto e = parse_return_expression().parse(toks, pos)) {
          LOG_DEBUG("Parsed return expression");
          return *e;
        }
        pos = saved;
        if (auto e = parse_call_expression().parse(toks, pos)) {
          LOG_DEBUG("Parsed call expression");
          return *e;
        }
        pos = saved;
        if (auto e = parse_loop_expression().parse(toks, pos)) {
          LOG_DEBUG("Parsed loop expression");
          return *e;
        }
        pos = saved;
        if (auto e = parse_while_expression().parse(toks, pos)) {
          LOG_DEBUG("Parsed while expression");
          return *e;
        }
        pos = saved;
        if (auto e = parse_block_expression().parse(toks, pos)) {
          LOG_DEBUG("Parsed block expression");
          return *e;
        }
        pos = saved;
        if (auto e = parse_primary_literal_expression().parse(toks, pos)) {
          LOG_DEBUG("Parsed primary literal expression");
          return *e;
        }
        pos = saved;
        LOG_DEBUG("Falling back to Pratt expression parser");
        return pratt_expression().parse(toks, pos);
      });
}

inline parsec::Parser<std::shared_ptr<Expression>>
Parser::parse_primary_literal_expression() {
  auto as_expr = [](const rc::Token &t) {
    LOG_DEBUG("Creating name expression from token: " + t.lexeme);
    return std::shared_ptr<Expression>(
        std::make_shared<NameExpression>(t.lexeme));
  };
  auto p = tok(TokenType::STRING_LITERAL).map([&](const rc::Token &t) {
    return as_expr(t);
  });
  auto p2 = tok(TokenType::CHAR_LITERAL).map([&](const rc::Token &t) {
    return as_expr(t);
  });
  auto p3 = tok(TokenType::C_STRING_LITERAL).map([&](const rc::Token &t) {
    return as_expr(t);
  });
  auto p4 = tok(TokenType::BYTE_STRING_LITERAL).map([&](const rc::Token &t) {
    return as_expr(t);
  });
  auto p5 = tok(TokenType::BYTE_LITERAL).map([&](const rc::Token &t) {
    return as_expr(t);
  });
  auto p6 =
      tok(TokenType::TRUE).map([&](const rc::Token &t) { return as_expr(t); });
  auto p7 =
      tok(TokenType::FALSE).map([&](const rc::Token &t) { return as_expr(t); });

  return parsec::Parser<std::shared_ptr<Expression>>(
      [p, p2, p3, p4, p5, p6,
       p7](const std::vector<rc::Token> &toks,
           size_t &pos) -> parsec::ParseResult<std::shared_ptr<Expression>> {
        size_t saved = pos;
        LOG_DEBUG("Attempting to parse primary literal at position " +
                  std::to_string(pos));
        if (auto r = p.parse(toks, pos)) {
          LOG_DEBUG("Parsed string literal");
          return *r;
        }
        pos = saved;
        if (auto r = p2.parse(toks, pos)) {
          LOG_DEBUG("Parsed char literal");
          return *r;
        }
        pos = saved;
        if (auto r = p3.parse(toks, pos)) {
          LOG_DEBUG("Parsed C string literal");
          return *r;
        }
        pos = saved;
        if (auto r = p4.parse(toks, pos)) {
          LOG_DEBUG("Parsed byte string literal");
          return *r;
        }
        pos = saved;
        if (auto r = p5.parse(toks, pos)) {
          LOG_DEBUG("Parsed byte literal");
          return *r;
        }
        pos = saved;
        if (auto r = p6.parse(toks, pos)) {
          LOG_DEBUG("Parsed true literal");
          return *r;
        }
        pos = saved;
        if (auto r = p7.parse(toks, pos)) {
          LOG_DEBUG("Parsed false literal");
          return *r;
        }
        pos = saved;
        LOG_DEBUG("No primary literal found");
        return std::nullopt;
      });
}

inline parsec::Parser<std::pair<std::string, LiteralType>>
Parser::identifier_and_type_parser() {
  using namespace parsec;
  return parsec::identifier.combine(
      optional(tok(TokenType::COLON).thenR(typ)),
      [](const auto &id, const auto &t) {
        const auto &ty =
            t.value_or(LiteralType(PrimitiveLiteralType::TO_BE_INFERRED));
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

} // namespace rc