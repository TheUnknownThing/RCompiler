#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../lexer/lexer.hpp"

#include "nodes/base.hpp"
#include "nodes/expr.hpp"
#include "nodes/stmt.hpp"
#include "nodes/topLevel.hpp"

namespace rc {
class Parser {
public:
  Parser(std::vector<Token> tokens);
  std::unique_ptr<RootNode> parse();

private:
  std::vector<Token> tokens;
  size_t pos = 0;

  std::unique_ptr<Statement> parse_statement();
  std::unique_ptr<LetStatement> parse_let_statement();
  std::unique_ptr<Expression> parse_expression();

  Token &peek();
  Token &previous();
  Token &advance();
  bool is_at_end();
  bool check(TokenType type);
  bool match(TokenType type);
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

} // namespace rc