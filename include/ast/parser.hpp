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

  std::unique_ptr<BaseNode> parse_statement();
  std::unique_ptr<BaseNode> parse_expression();
};

inline Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

inline std::unique_ptr<RootNode> Parser::parse() {
  // TODO
}

inline std::unique_ptr<BaseNode> Parser::parse_statement() {
  auto tbl = pratt::default_table();
  auto expr = pratt::pratt_expr(tbl);

  auto let_stmt =
      tok(TokenType::LET)
          .thenR(identifier)
          .thenL(tok(TokenType::COLON))
          .combine(typ,
                   [](auto name, auto ty) { return std::make_pair(name, ty); })
          .thenL(tok(TokenType::EQ))
          .combine(expr, [](auto pair, auto e) {
            return LetStatement{pair.first, pair.second, e};
          });

  
}

inline std::unique_ptr<BaseNode> Parser::parse_expression() {
  // TODO
}

/*

Deprecated Parser Class

class Parser {
public:
  Parser(std::vector<Token> tokens);
  std::unique_ptr<RootNode> parse();

private:
  std::vector<Token> tokens;
  size_t pos = 0;

  std::unique_ptr<BaseNode> parse_statement();
  std::unique_ptr<BaseNode> parse_expression();

  std::unique_ptr<FunctionDecl> parse_function_declaration();
  std::unique_ptr<StructDecl> parse_struct_declaration();
  std::unique_ptr<BlockStatement> parse_block_statement();
  std::unique_ptr<LetStatement> parse_let_statement();

  Token &peek();
  Token &previous();
  Token &advance();
  bool is_at_end();

  bool check(TokenType type);

  bool match(TokenType type);

  bool matchLiteralType(LiteralType type);
};

inline Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

inline std::unique_ptr<RootNode> Parser::parse() {
  auto root = std::make_unique<RootNode>();
  while (!is_at_end()) {
    auto stmt = parse_statement();
    if (stmt) {
      root->children.push_back(std::move(stmt));
    }
  }
  return root;
}

inline Token &Parser::peek() { return tokens[pos]; }

inline Token &Parser::previous() { return tokens[pos - 1]; }

inline Token &Parser::advance() {
  if (!is_at_end()) {
    pos++;
  }
  return previous();
}

inline bool Parser::is_at_end() {
  return pos >= tokens.size() || tokens[pos].type == TokenType::TOK_EOF;
}

inline bool Parser::check(TokenType type) {
  return !is_at_end() && peek().type == type;
}

inline bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

inline std::unique_ptr<BaseNode> Parser::parse_statement() {
  if (match(TokenType::LET)) {
    return parse_let_statement();
  } else if (match(TokenType::FN)) {
    return parse_function_declaration();
  } else if (match(TokenType::STRUCT)) {
    return parse_struct_declaration();
  } else if (match(TokenType::L_BRACE)) {
    return parse_block_statement();
  } // TODO Here

  return nullptr;
}

inline std::unique_ptr<FunctionDecl> Parser::parse_function_declaration() {
  Token name;
  std::vector<Token> parameters;
  BlockStatement *body = nullptr;

  if (match(TokenType::NON_KEYWORD_IDENTIFIER)) {
    name = previous();
  } else {
    throw std::runtime_error("Expected function name");
  }

  if (match(TokenType::L_PAREN)) {
    while (!check(TokenType::R_PAREN) && !is_at_end()) {
      if (match(TokenType::NON_KEYWORD_IDENTIFIER)) {
        parameters.push_back(previous());
      } else {
        throw std::runtime_error("Expected parameter name");
      }
      if (!match(TokenType::COMMA)) {
        break;
      }
    }
    advance(); // Consume R_PAREN
  }

  if (match(TokenType::L_BRACE)) {
    // function body
    body = dynamic_cast<BlockStatement *>(parse_block_statement().release());
  } else if (match(TokenType::ARROW)) {
    // return type
    // TODO
  }
}
*/

} // namespace rc