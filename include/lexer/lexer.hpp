#pragma once

#include <regex>
#include <string>
#include <vector>

namespace rc {
enum class TokenType {
  TOK_EOF, // End of file

  // Strict keywords
  AS,        // as
  BREAK,     // break
  CONST,     // const
  CONTINUE,  // continue
  CRATE,     // crate
  ELSE,      // else
  ENUM,      // enum
  EXTERN,    // extern
  FALSE,     // false
  FN,        // fn
  FOR,       // for
  IF,        // if
  IMPL,      // impl
  IN,        // in
  LET,       // let
  LOOP,      // loop
  MATCH,     // match
  MOD,       // mod
  MOVE,      // move
  MUT,       // mut
  PUB,       // pub
  REF,       // ref
  RETURN,    // return
  SELF,      // self
  SELF_TYPE, // Self
  STATIC,    // static
  STRUCT,    // struct
  SUPER,     // super
  TRAIT,     // trait
  TRUE,      // true
  TYPE,      // type
  UNSAFE,    // unsafe
  USE,       // use
  WHERE,     // where
  WHILE,     // while
  ASYNC,     // async
  AWAIT,     // await
  DYN,       // dyn

  // Reserved keywords
  ABSTRACT, // abstract
  BECOME,   // become
  BOX,      // box
  DO,       // do
  FINAL,    // final
  MACRO,    // macro
  OVERRIDE, // override
  PRIV,     // priv
  TYPEOF,   // typeof
  UNSIZED,  // unsized
  VIRTUAL,  // virtual
  YIELD,    // yield
  TRY,      // try
  GEN,      // gen

  // Weak keywords
  STATIC_LIFETIME, // 'static
  MACRO_RULES,     // macro_rules
  RAW,             // raw
  SAFE,            // safe
  UNION,           // union

  // identifiers
  NON_KEYWORD_IDENTIFIER,
  RAW_IDENTIFIER,
  RESERVED_RAW_IDENTIFIER,

  FLOAT_LITERAL,
  CHAR_LITERAL,
  INTEGER_LITERAL,
  STRING_LITERAL,
  RAW_STRING_LITERAL,
  BYTE_LITERAL,
  RAW_BYTE_LITERAL,
  C_STRING_LITERAL,
  RAW_C_STRING_LITERAL,

  ASSIGN,    // =
  PLUS,      // +
  MINUS,     // -
  STAR,      // *
  SLASH,     // /
  PERCENT,   // %
  AMPERSAND, // &
  PIPE,      // |
  CARET,     // ^

  L_PAREN,     // (
  R_PAREN,     // )
  L_BRACE,     // {
  R_BRACE,     // }
  L_BRACKET,   // [
  R_BRACKET,   // ]
  COMMA,       // ,
  DOT,         // .
  COLON,       // :
  SEMICOLON,   // ;
  QUESTION,    // ?
  COLON_COLON, // ::

  UNKNOWN // unknown
};

static std::map<TokenType, std::string> tokenTypeToRegex = {
    {TokenType::TOK_EOF, R"(\s*EOF\s*)"},
    {TokenType::AS, R"(\s*as\s*)"},
    {TokenType::BREAK, R"(\s*break\s*)"},
    {TokenType::CONST, R"(\s*const\s*)"},
    {TokenType::CONTINUE, R"(\s*continue\s*)"},
    {TokenType::CRATE, R"(\s*crate\s*)"},
    {TokenType::ELSE, R"(\s*else\s*)"},
    {TokenType::ENUM, R"(\s*enum\s*)"},
    {TokenType::EXTERN, R"(\s*extern\s*)"},
    {TokenType::FALSE, R"(\s*false\s*)"},
    {TokenType::FN, R"(\s*fn\s*)"},
    {TokenType::FOR, R"(\s*for\s*)"},
    {TokenType::IF, R"(\s*if\s*)"},
    {TokenType::IMPL, R"(\s*impl\s*)"},
    {TokenType::IN, R"(\s*in\s*)"},
    {TokenType::LET, R"(\s*let\s*)"},
    {TokenType::LOOP, R"(\s*loop\s*)"},
    {TokenType::MATCH, R"(\s*match\s*)"},
    {TokenType::MOD, R"(\s*mod\s*)"},
    {TokenType::MOVE, R"(\s*move\s*)"},
    {TokenType::MUT, R"(\s*mut\s*)"},
    {TokenType::PUB, R"(\s*pub\s*)"},
    {TokenType::REF, R"(\s*ref\s*)"},
    {TokenType::RETURN, R"(\s*return\s*)"},
    {TokenType::SELF, R"(\s*self\s*)"},
    {TokenType::SELF_TYPE, R"(\s*Self\s*)"},
    {TokenType::STATIC, R"(\s*static\s*)"},
    {TokenType::STRUCT, R"(\s*struct\s*)"},
    {TokenType::SUPER, R"(\s*super\s*)"},
    {TokenType::TRAIT, R"(\s*trait\s*)"},
    {TokenType::TRUE, R"(\s*true\s*)"},
    {TokenType::TYPE, R"(\s*type\s*)"},
    {TokenType::UNSAFE, R"(\s*unsafe\s*)"},
    {TokenType::USE, R"(\s*use\s*)"},
    {TokenType::WHERE, R"(\s*where\s*)"},
    {TokenType::WHILE, R"(\s*while\s*)"},
    {TokenType::ASYNC, R"(\s*async\s*)"},
    {TokenType::AWAIT, R"(\s*await\s*)"},
    {TokenType::DYN, R"(\s*dyn\s*)"},
    {TokenType::ABSTRACT, R"(\s*abstract\s*)"},
    {TokenType::BECOME, R"(\s*become\s*)"},
    {TokenType::BOX, R"(\s*box\s*)"},
    {TokenType::DO, R"(\s*do\s*)"},
    {TokenType::FINAL, R"(\s*final\s*)"},
    {TokenType::MACRO, R"(\s*macro\s*)"},
    {TokenType::OVERRIDE, R"(\s*override\s*)"},
    {TokenType::PRIV, R"(\s*priv\s*)"},
    {TokenType::TYPEOF, R"(\s*typeof\s*)"},
    {TokenType::UNSIZED, R"(\s*unsized\s*)"},
    {TokenType::VIRTUAL, R"(\s*virtual\s*)"},
    {TokenType::YIELD, R"(\s*yield\s*)"},
    {TokenType::TRY, R"(\s*try\s*)"},
    {TokenType::GEN, R"(\s*gen\s*)"},
    {TokenType::STATIC_LIFETIME, R"(\s*'static\s*)"},
    {TokenType::MACRO_RULES, R"(\s*macro_rules\s*)"},
    {TokenType::RAW, R"(\s*raw\s*)"},
    {TokenType::SAFE, R"(\s*safe\s*)"},
    {TokenType::UNION, R"(\s*union\s*)"},
    {TokenType::NON_KEYWORD_IDENTIFIER, R"(\s*[a-zA-Z_][a-zA-Z0-9_]*\b)"},
    {TokenType::RAW_IDENTIFIER, R"(\s*r#?[a-zA-Z_][a-zA-Z0-9_]*\b)"},
    {TokenType::RESERVED_RAW_IDENTIFIER,
     R"(\s*r#?self\b|\s*r#?Self\b|\s*r#?super\b)"},
    {TokenType::FLOAT_LITERAL, R"(\s*\d+\.\d+([eE][+-]?\d+)?\b)"},
    {TokenType::CHAR_LITERAL, R"(\s*'([^'\\]|\\.)'\b)"},
    {TokenType::INTEGER_LITERAL, R"(\s*\d+\b)"},
    {TokenType::STRING_LITERAL, R"(\s*"([^"\\]|\\.)*"\b)"},
    {TokenType::RAW_STRING_LITERAL, R"(\s*r#?"([^"#]|#")*"#\b)"},
    {TokenType::BYTE_LITERAL, R"(\s*b'([^'\\]|\\.)'\b)"},
    {TokenType::RAW_BYTE_LITERAL, R"(\s*br'([^'\\]|\\.)*'\b)"},
    {TokenType::C_STRING_LITERAL, R"(\s*c"([^"\\]|\\.)*"\b)"},
    {TokenType::RAW_C_STRING_LITERAL, R"(\s*cr"([^"\\]|\\.)*"\b)"},
    {TokenType::ASSIGN, R"(\s*=\s*)"},
    {TokenType::PLUS, R"(\s*\+\s*)"},
    {TokenType::MINUS, R"(\s*-\s*)"},
    {TokenType::STAR, R"(\s*\*\s*)"},
    {TokenType::SLASH, R"(\s*/\s*)"},
    {TokenType::PERCENT, R"(\s*%\s*)"},
    {TokenType::AMPERSAND, R"(\s*&\s*)"},
    {TokenType::PIPE, R"(\s*\|\s*)"},
    {TokenType::CARET, R"(\s*\^\s*)"},
    {TokenType::L_PAREN, R"(\s*\(\s*)"},
    {TokenType::R_PAREN, R"(\s*\)\s*)"},
    {TokenType::L_BRACE, R"(\s*\{\s*)"},
    {TokenType::R_BRACE, R"(\s*\}\s*)"},
    {TokenType::L_BRACKET, R"(\s*\[\s*)"},
    {TokenType::R_BRACKET, R"(\s*\]\s*)"},
    {TokenType::COMMA, R"(\s*,\s*)"},
    {TokenType::DOT, R"(\s*\.\s*)"},
    {TokenType::COLON, R"(\s*:\s*)"},
    {TokenType::SEMICOLON, R"(\s*;\s*)"},
    {TokenType::QUESTION, R"(\s*\?\s*)"},
    {TokenType::COLON_COLON, R"(\s*:\:\s*)"},
    {TokenType::UNKNOWN, R"(\S+)"}};

struct Token {
  TokenType type;
  std::string lexeme;
};

class Lexer {
public:
  Lexer(const std::vector<std::string> &source);
  std::vector<Token> tokenize();

private:
  std::vector<std::string> source;
};

inline Lexer::Lexer(const std::vector<std::string> &source) : source(source) {}

inline std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  for (const auto &line : source) {
    // placeholder
  }
  return tokens;
}

} // namespace rc