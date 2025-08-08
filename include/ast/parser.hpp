#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../lexer/lexer.hpp"
#include "types.hpp"

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

  /**
   * @brief Check if the next token is of the specified type.
   * @param type The token type to check against.
   * @return true if the next token matches the type, false otherwise.
   */
  bool check(TokenType type);

  /**
   * @brief Consume the next token if it matches the specified type.
   * @return true if a token was consumed, false otherwise.
   */
  bool match(TokenType type);

  /**
   * @brief Check if the literal type matches one of the known types.
   * @param type The literal type to check.
   * @return true if the type is a known literal type, false otherwise.
   */
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
  std::string name;
  std::vector<std::string> parameters;
  BlockStatement *body = nullptr;

  if (match(TokenType::NON_KEYWORD_IDENTIFIER)) {
    name = previous().lexeme;
  } else {
    throw std::runtime_error("Expected function name");
  }

  if (match(TokenType::L_PAREN)) {
    while (!check(TokenType::R_PAREN) && !is_at_end()) {
      if (match(TokenType::NON_KEYWORD_IDENTIFIER)) {
        parameters.push_back(previous().lexeme);
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

} // namespace rc