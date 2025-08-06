#pragma once

#include <regex>
#include <string>
#include <type_traits>
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

  // literals
  INTEGER_LITERAL,

  CHAR_LITERAL,
  STRING_LITERAL,
  C_STRING_LITERAL,
  BYTE_STRING_LITERAL,
  // RAW_STRING_LITERAL,
  // RAW_C_STRING_LITERAL,
  // RAW_BYTE_STRING_LITERAL,
  BYTE_LITERAL,

  // operators
  SHL_EQ,       // <<=
  SHR_EQ,       // >>=
  COLON_COLON,  // ::
  FAT_ARROW,    // =>
  ARROW,        // ->
  LE,           // <=
  GE,           // >=
  EQ,           // ==
  NE,           // !=
  AND,          // &&
  OR,           // ||
  PLUS_EQ,      // +=
  MINUS_EQ,     // -=
  STAR_EQ,      // *=
  SLASH_EQ,     // /=
  PERCENT_EQ,   // %=
  AMPERSAND_EQ, // &=
  PIPE_EQ,      // |=
  CARET_EQ,     // ^=
  SHL,          // <<
  SHR,          // >>

  ASSIGN,    // =
  PLUS,      // +
  MINUS,     // -
  STAR,      // *
  SLASH,     // /
  PERCENT,   // %
  AMPERSAND, // &
  PIPE,      // |
  CARET,     // ^
  NOT,       // !
  QUESTION,  // ?
  LT,        // <
  GT,        // >

  // punctuation
  L_PAREN,   // (
  R_PAREN,   // )
  L_BRACE,   // {
  R_BRACE,   // }
  L_BRACKET, // [
  R_BRACKET, // ]
  COMMA,     // ,
  DOT,       // .
  COLON,     // :
  SEMICOLON, // ;

  UNKNOWN // unknown
};

static std::map<TokenType, std::string> tokenTypeToRegex = {
    // Strict keywords
    {TokenType::AS, R"(\bas\b)"},
    {TokenType::BREAK, R"(\bbreak\b)"},
    {TokenType::CONST, R"(\bconst\b)"},
    {TokenType::CONTINUE, R"(\bcontinue\b)"},
    {TokenType::CRATE, R"(\bcrate\b)"},
    {TokenType::ELSE, R"(\belse\b)"},
    {TokenType::ENUM, R"(\benum\b)"},
    {TokenType::EXTERN, R"(\bextern\b)"},
    {TokenType::FALSE, R"(\bfalse\b)"},
    {TokenType::FN, R"(\bfn\b)"},
    {TokenType::FOR, R"(\bfor\b)"},
    {TokenType::IF, R"(\bif\b)"},
    {TokenType::IMPL, R"(\bimpl\b)"},
    {TokenType::IN, R"(\bin\b)"},
    {TokenType::LET, R"(\blet\b)"},
    {TokenType::LOOP, R"(\bloop\b)"},
    {TokenType::MATCH, R"(\bmatch\b)"},
    {TokenType::MOD, R"(\bmod\b)"},
    {TokenType::MOVE, R"(\bmove\b)"},
    {TokenType::MUT, R"(\bmut\b)"},
    {TokenType::PUB, R"(\bpub\b)"},
    {TokenType::REF, R"(\bref\b)"},
    {TokenType::RETURN, R"(\breturn\b)"},
    {TokenType::SELF, R"(\bself\b)"},
    {TokenType::SELF_TYPE, R"(\bSelf\b)"},
    {TokenType::STATIC, R"(\bstatic\b)"},
    {TokenType::STRUCT, R"(\bstruct\b)"},
    {TokenType::SUPER, R"(\bsuper\b)"},
    {TokenType::TRAIT, R"(\btrait\b)"},
    {TokenType::TRUE, R"(\btrue\b)"},
    {TokenType::TYPE, R"(\btype\b)"},
    {TokenType::UNSAFE, R"(\bunsafe\b)"},
    {TokenType::USE, R"(\buse\b)"},
    {TokenType::WHERE, R"(\bwhere\b)"},
    {TokenType::WHILE, R"(\bwhile\b)"},
    {TokenType::ASYNC, R"(\basync\b)"},
    {TokenType::AWAIT, R"(\bawait\b)"},
    {TokenType::DYN, R"(\bdyn\b)"},

    // Reserved keywords
    {TokenType::ABSTRACT, R"(\babstract\b)"},
    {TokenType::BECOME, R"(\bbecome\b)"},
    {TokenType::BOX, R"(\bbox\b)"},
    {TokenType::DO, R"(\bdo\b)"},
    {TokenType::FINAL, R"(\bfinal\b)"},
    {TokenType::MACRO, R"(\bmacro\b)"},
    {TokenType::OVERRIDE, R"(\boverride\b)"},
    {TokenType::PRIV, R"(\bpriv\b)"},
    {TokenType::TYPEOF, R"(\btypeof\b)"},
    {TokenType::UNSIZED, R"(\bunsized\b)"},
    {TokenType::VIRTUAL, R"(\bvirtual\b)"},
    {TokenType::YIELD, R"(\byield\b)"},
    {TokenType::TRY, R"(\btry\b)"},
    {TokenType::GEN, R"(\bgen\b)"}};

struct Token {
  TokenType type;
  std::string lexeme;
};

class Lexer {
public:
  Lexer(const std::string &source);
  std::vector<Token> tokenize();

private:
  std::string source;
  std::vector<Token> tokens;

  void firstPass();
  bool checkWordBoundary(char c);
  bool isPunctuation(char c);
  void checkForKeywords();
};

inline Lexer::Lexer(const std::string &source) : source(source) {}

inline std::vector<Token> Lexer::tokenize() {
  firstPass();
  checkForKeywords();
  return tokens;
}

inline bool Lexer::checkWordBoundary(char c) {
  if (std::isalnum(c) || c == '_') {
    return true;
  }
  return false;
}

inline bool Lexer::isPunctuation(char c) {
  return c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
         c == ',' || c == '.' || c == ':' || c == ';';
}

inline void Lexer::firstPass() {
  bool in_string = false;
  bool in_char = false;
  bool escaped = false;
  bool in_integer = false;
  bool in_operator = false;
  bool in_identifier = false;

  size_t start_pos = 0;

  for (size_t i = 0; i < source.length(); ++i) {
    char c = source[i];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        in_string = false;
        tokens.push_back({TokenType::STRING_LITERAL,
                          source.substr(start_pos, i - start_pos + 1)});
      }
    } else if (in_char) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '\'') {
        in_char = false;
        tokens.push_back({TokenType::CHAR_LITERAL,
                          source.substr(start_pos, i - start_pos + 1)});
      }
    } else if (in_integer) {
      if (checkWordBoundary(c)) {
        continue;
      } else {
        in_integer = false;
        tokens.push_back({TokenType::INTEGER_LITERAL,
                          source.substr(start_pos, i - start_pos)});
        i--;
      }
    } else if (in_identifier) {
      if (checkWordBoundary(c)) {
        continue;
      } else {
        in_identifier = false;
        tokens.push_back({TokenType::NON_KEYWORD_IDENTIFIER,
                          source.substr(start_pos, i - start_pos)});
        i--;
      }
    } else if (in_operator) {
      if (c == '=' || c == '!' || c == '<' || c == '>' || c == '&' ||
          c == '|' || c == '+' || c == '-' || c == '*' || c == '/' ||
          c == '%' || c == '^') {
        continue;
      } else {
        in_operator = false;
        if (start_pos < i) {
          tokens.push_back(
              {TokenType::UNKNOWN, source.substr(start_pos, i - start_pos)});
        }
        i--;
      }
    } else if (isPunctuation(c)) {
      switch (c) {
      case '(':
        tokens.push_back({TokenType::L_PAREN, "("});
        break;
      case ')':
        tokens.push_back({TokenType::R_PAREN, ")"});
        break;
      case '{':
        tokens.push_back({TokenType::L_BRACE, "{"});
        break;
      case '}':
        tokens.push_back({TokenType::R_BRACE, "}"});
        break;
      case '[':
        tokens.push_back({TokenType::L_BRACKET, "["});
        break;
      case ']':
        tokens.push_back({TokenType::R_BRACKET, "]"});
        break;
      case ',':
        tokens.push_back({TokenType::COMMA, ","});
        break;
      case '.':
        tokens.push_back({TokenType::DOT, "."});
        break;
      case ':':
        tokens.push_back({TokenType::COLON, ":"});
        break;
      case ';':
        tokens.push_back({TokenType::SEMICOLON, ";"});
        break;
      default:
        tokens.push_back({TokenType::UNKNOWN, std::string(1, c)});
      }
      start_pos = i + 1;
    } else {
      if (c == '"') {
        in_string = true;
        start_pos = i;
      } else if (c == '\'') {
        in_char = true;
        start_pos = i;
      } else if (c == '0' || (c >= '1' && c <= '9')) {
        if (!in_integer) {
          in_integer = true;
          start_pos = i;
        }
      } else if (std::isalpha(c)) {
        if (!in_identifier) {
          in_identifier = true;
          start_pos = i;
        }
      } else if (std::isspace(c)) {
        if (in_string || in_char || in_integer || in_identifier) {
          if (in_string) {
            tokens.push_back({TokenType::STRING_LITERAL,
                              source.substr(start_pos, i - start_pos)});
          } else if (in_char) {
            tokens.push_back({TokenType::CHAR_LITERAL,
                              source.substr(start_pos, i - start_pos)});
          } else if (in_integer) {
            tokens.push_back({TokenType::INTEGER_LITERAL,
                              source.substr(start_pos, i - start_pos)});
          } else if (in_identifier) {
            tokens.push_back({TokenType::NON_KEYWORD_IDENTIFIER,
                              source.substr(start_pos, i - start_pos)});
          } else if (in_operator) {
            tokens.push_back(
                {TokenType::UNKNOWN, source.substr(start_pos, i - start_pos)});
          }
          start_pos = i + 1;
          in_string = false;
          in_char = false;
          in_integer = false;
          in_identifier = false;
          in_operator = false;
        }
      }
    }
  }
}

inline void Lexer::checkForKeywords() {
  for (auto &tok : tokens) {
    if (tok.type == TokenType::NON_KEYWORD_IDENTIFIER) {
      for (const auto &[type, pattern] : tokenTypeToRegex) {
        std::regex regex(pattern);
        if (std::regex_match(tok.lexeme, regex)) {
          tok.type = type;
          break;
        }
      }
    }
  }
}

} // namespace rc