#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../lexer/lexer.hpp"
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

  // Helper
  parsec::Parser<std::pair<std::string, LiteralType>>
  identifier_and_type_parser();
  parsec::Parser<std::vector<std::pair<std::string, LiteralType>>>
  argument_list_parser();
};

inline Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

inline std::unique_ptr<RootNode> Parser::parse() {
  auto root = std::make_unique<RootNode>();
  auto tbl = pratt::default_table();
  auto expr = pratt::pratt_expr(tbl);

  size_t pos = 0;
  while (pos < tokens.size() && tokens[pos].type != TokenType::TOK_EOF) {
    std::vector<Token> sub_tokens(tokens.begin() + pos, tokens.end());
    Parser sub_parser(sub_tokens);
    if (auto item = sub_parser.parse_item()) {
      root->children.push_back(std::move(item));
      // Advance position - this is simplified, in real implementation we'd
      // track how many tokens were consumed
      pos++;
      continue;
    }
  }

  return root;
}

inline std::unique_ptr<BaseNode> Parser::parse_item() {
  if (tokens.empty())
    return nullptr;

  // TODO
}

inline std::unique_ptr<BaseNode> Parser::parse_statement() {
  auto tbl = pratt::default_table();
  auto expr = pratt::pratt_expr(tbl);

  size_t pos = 0;
  if (pos < tokens.size()) {
    if (tokens[pos].type == TokenType::SEMICOLON) {
      pos++;
      return std::make_unique<EmptyStatement>();
    }

    // expression statement
    if (auto e = expr.parse(tokens, pos)) {
      bool has_semicolon = false;
      if (pos < tokens.size() && tokens[pos].type == TokenType::SEMICOLON) {
        pos++;
        has_semicolon = true;
      }
      return std::make_unique<ExpressionStatement>(*e, has_semicolon);
    }
  }

  return nullptr;
}

inline std::unique_ptr<BaseNode> Parser::parse_expression() {
  // TODO
  return nullptr;
}

inline parsec::Parser<std::unique_ptr<FunctionDecl>> Parser::parse_function() {
  auto identifier_and_type = identifier_and_type_parser();
  auto argument_list = argument_list_parser();
  auto return_type = tok(TokenType::ARROW).thenR(typ);

  return tok(TokenType::FN)
      .thenR(identifier)
      .combine(optional(argument_list),
               [](const auto &name, const auto &params) {
                 return std::make_pair(name, params);
               })
      .combine(optional(return_type), [](const auto &pm_list, const auto &ty) {
        auto ret_ty =
            ty.value_or(LiteralType(PrimitiveLiteralType::TO_BE_INFERRED));
        return std::make_unique<FunctionDecl>(pm_list.first, pm_list.second,
                                              ret_ty);
      });
}

inline parsec::Parser<std::pair<std::string, LiteralType>>
Parser::identifier_and_type_parser() {
  using namespace parsec;
  return identifier.combine(optional(tok(TokenType::COLON).thenR(typ)),
                            [](const auto &id, const auto &t) {
                              const auto &ty = t.value_or(LiteralType(
                                  PrimitiveLiteralType::TO_BE_INFERRED));
                              return std::make_pair(id, ty);
                            });
}

inline parsec::Parser<std::vector<std::pair<std::string, LiteralType>>>
Parser::argument_list_parser() {
  using namespace parsec;
  auto identifier_and_type = identifier_and_type_parser();

  return tok(TokenType::L_PAREN)
      .thenR(many(identifier_and_type.thenL(optional(tok(TokenType::COMMA)))))
      .thenL(tok(TokenType::R_PAREN));
}

} // namespace rc