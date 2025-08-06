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
  COLON_COLON, // ::
  FAT_ARROW,   // =>
  ARROW,       // ->
  LE,          // <=
  GE,          // >=
  EQ,          // ==
  NE,          // !=
  AND,         // &&
  OR,          // ||

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
    {TokenType::GEN, R"(\bgen\b)"},

    // Weak keywords
    // BUGGY!!!!!
    {TokenType::STATIC_LIFETIME, R"(\s*'static\s*)"},
    {TokenType::MACRO_RULES, R"(\s*macro_rules\s*)"},
    {TokenType::RAW, R"(\s*raw\s*)"},
    {TokenType::SAFE, R"(\s*safe\s*)"},
    {TokenType::UNION, R"(\s*union\s*)"},

    // literals
    {TokenType::INTEGER_LITERAL, R"(\b(?:0[xX][0-9a-fA-F]+|0[oO][0-7]+|0[bB][01]+|\d+)[uiUL]*\b)"},

    {TokenType::CHAR_LITERAL, R"(\s*'([^'\\]|\\.)'\b)"},
    {TokenType::STRING_LITERAL, R"(\s*"([^"\\]|\\.)*"\b)"},
    {TokenType::C_STRING_LITERAL, R"(\s*c"([^"\\]|\\.)*"\b)"},
    {TokenType::BYTE_STRING_LITERAL, R"(\s*b"([^"\\]|\\.)*"\b)"},
    // {TokenType::RAW_STRING_LITERAL, R"(\s*r"([^"]|\\")*"\b)"},
    // {TokenType::RAW_C_STRING_LITERAL, R"(\s*r"c([^"]|\\")*"\b)"},
    // {TokenType::RAW_BYTE_STRING_LITERAL, R"(\s*r"b([^"]|\\")*"\b)"},
    {TokenType::BYTE_LITERAL, R"(\s*b'([^'\\]|\\.)'\b)"},
    
    // operators
    {TokenType::COLON_COLON, R"(::)"},
    {TokenType::FAT_ARROW, R"(=>)"},
    {TokenType::ARROW, R"(->)"},
    {TokenType::LE, R"(<=)"},
    {TokenType::GE, R"(>=)"},
    {TokenType::EQ, R"(==)"},
    {TokenType::NE, R"(!=)"},
    {TokenType::AND, R"(&&)"},
    {TokenType::OR, R"(\|\|)"},

    {TokenType::ASSIGN, R"(=)"},
    {TokenType::PLUS, R"(\+)"},
    {TokenType::MINUS, R"(-)"},
    {TokenType::STAR, R"(\*)"},
    {TokenType::SLASH, R"(/)"},
    {TokenType::PERCENT, R"(%)"},
    {TokenType::AMPERSAND, R"(&)"},
    {TokenType::PIPE, R"(\|)"},
    {TokenType::CARET, R"(\^)"},
    {TokenType::NOT, R"(!)"},
    {TokenType::QUESTION, R"(\?)"},

    {TokenType::L_PAREN, R"(\()"},
    {TokenType::R_PAREN, R"(\))"},
    {TokenType::L_BRACE, R"(\{)"},
    {TokenType::R_BRACE, R"(\})"},
    {TokenType::L_BRACKET, R"(\[)"},
    {TokenType::R_BRACKET, R"(\])"},
    {TokenType::COMMA, R"(,)"},
    {TokenType::DOT, R"(\.)"},
    {TokenType::COLON, R"(:)"},
    {TokenType::SEMICOLON, R"(;)"},

    // identifiers
    {TokenType::NON_KEYWORD_IDENTIFIER, R"(\b[a-zA-Z][a-zA-Z0-9_]\b)"},

    {TokenType::UNKNOWN, R"(\S+)"}};

struct Token {
  TokenType type;
  std::string lexeme;
};

struct StringToken {
  TokenType type;
  std::string value;
  size_t start_pos;
  size_t end_pos;
};

class Lexer {
public:
  Lexer(const std::string &source);
  std::vector<Token> tokenize();

private:
  std::string source;
  std::vector<StringToken> string_tokens;
  void firstPass();
  void checkForKeywords();
  void extractStringTokens(const std::string &line);
};

inline Lexer::Lexer(const std::string &source) : source(source) {}

inline std::vector<Token> Lexer::tokenize() {
  extractStringTokens(source);

  std::vector<Token> tokens;

  std::regex token_regex;
  size_t pos = 0;
  for (const auto &it : string_tokens) {
  }

  return tokens;
}

inline void Lexer::extractStringTokens(const std::string &line) {
  bool in_string = false;
  bool in_char = false;
  bool escaped = false;
  size_t start_pos = 0;

  for (size_t i = 0; i < line.length(); ++i) {
    char c = line[i];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        in_string = false;
        string_tokens.push_back({TokenType::STRING_LITERAL,
                                 line.substr(start_pos, i - start_pos + 1),
                                 start_pos, i});
      }
    } else if (in_char) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '\'') {
        in_char = false;
        string_tokens.push_back({TokenType::CHAR_LITERAL,
                                 line.substr(start_pos, i - start_pos + 1),
                                 start_pos, i});
      }
    } else {
      if (c == '"') {
        in_string = true;
        start_pos = i;
      } else if (c == '\'') {
        in_char = true;
        start_pos = i;
      }
    }
  }
}

inline void Lexer::firstPass() {
  
}

} // namespace rc