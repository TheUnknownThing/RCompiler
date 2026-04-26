#include "lexer/lexer.hpp"

namespace rc {

const char *to_string(TokenType type) {
  switch (type) {
#define X(name, str)                                                           \
  case TokenType::name:                                                        \
    return str;
#include "lexer/token_defs.def"
#undef X
  default:
    return "!!_UNDEFINED_TOKEN_!!";
  }
}

std::ostream &operator<<(std::ostream &os, const TokenType &type) {
  os << to_string(type);
  return os;
}
const std::unordered_map<std::string_view, TokenType>
    keyword_to_token_type = {
        // Strict keywords
        {"as", TokenType::AS},
        {"break", TokenType::BREAK},
        {"const", TokenType::CONST},
        {"continue", TokenType::CONTINUE},
        {"crate", TokenType::CRATE},
        {"else", TokenType::ELSE},
        {"enum", TokenType::ENUM},
        {"extern", TokenType::EXTERN},
        {"false", TokenType::FALSE},
        {"fn", TokenType::FN},
        {"for", TokenType::FOR},
        {"if", TokenType::IF},
        {"impl", TokenType::IMPL},
        {"in", TokenType::IN},
        {"let", TokenType::LET},
        {"loop", TokenType::LOOP},
        {"match", TokenType::MATCH},
        {"mod", TokenType::MOD},
        {"move", TokenType::MOVE},
        {"mut", TokenType::MUT},
        {"pub", TokenType::PUB},
        {"ref", TokenType::REF},
        {"return", TokenType::RETURN},
        {"self", TokenType::SELF},
        {"Self", TokenType::SELF_TYPE},
        {"static", TokenType::STATIC},
        {"struct", TokenType::STRUCT},
        {"super", TokenType::SUPER},
        {"trait", TokenType::TRAIT},
        {"true", TokenType::TRUE},
        {"type", TokenType::TYPE},
        {"unsafe", TokenType::UNSAFE},
        {"use", TokenType::USE},
        {"where", TokenType::WHERE},
        {"while", TokenType::WHILE},
        {"async", TokenType::ASYNC},
        {"await", TokenType::AWAIT},
        {"dyn", TokenType::DYN},

        // Reserved keywords
        {"abstract", TokenType::ABSTRACT},
        {"become", TokenType::BECOME},
        {"box", TokenType::BOX},
        {"do", TokenType::DO},
        {"final", TokenType::FINAL},
        {"macro", TokenType::MACRO},
        {"override", TokenType::OVERRIDE},
        {"priv", TokenType::PRIV},
        {"typeof", TokenType::TYPEOF},
        {"unsized", TokenType::UNSIZED},
        {"virtual", TokenType::VIRTUAL},
        {"yield", TokenType::YIELD},
        {"try", TokenType::TRY},
        {"gen", TokenType::GEN},
};
const std::unordered_map<std::string_view, TokenType>
    operator_to_token_type = {
        {"+", TokenType::PLUS},
        {"-", TokenType::MINUS},
        {"*", TokenType::STAR},
        {"/", TokenType::SLASH},
        {"%", TokenType::PERCENT},
        {"&", TokenType::AMPERSAND},
        {"|", TokenType::PIPE},
        {"^", TokenType::CARET},
        {"!", TokenType::NOT},
        {"?", TokenType::QUESTION},
        {"=", TokenType::ASSIGN},
        {"<", TokenType::LT},
        {">", TokenType::GT},
        {"<<", TokenType::SHL},
        {">>", TokenType::SHR},
        {"<<=", TokenType::SHL_EQ},
        {">>=", TokenType::SHR_EQ},
        {"=>", TokenType::FAT_ARROW},
        {"->", TokenType::ARROW},
        {"<=", TokenType::LE},
        {">=", TokenType::GE},
        {"==", TokenType::EQ},
        {"!=", TokenType::NE},
        {"&&", TokenType::AND},
        {"||", TokenType::OR},
        {"+=", TokenType::PLUS_EQ},
        {"-=", TokenType::MINUS_EQ},
        {"*=", TokenType::STAR_EQ},
        {"/=", TokenType::SLASH_EQ},
        {"%=", TokenType::PERCENT_EQ},
        {"&=", TokenType::AMPERSAND_EQ},
        {"|=", TokenType::PIPE_EQ},
        {"^=", TokenType::CARET_EQ},
};
const std::unordered_map<std::string_view, TokenType>
    punctuation_to_token_type = {{"::", TokenType::COLON_COLON},
                              {"..", TokenType::DOT_DOT},
                              {"(", TokenType::L_PAREN},
                              {")", TokenType::R_PAREN},
                              {"{", TokenType::L_BRACE},
                              {"}", TokenType::R_BRACE},
                              {"[", TokenType::L_BRACKET},
                              {"]", TokenType::R_BRACKET},
                              {",", TokenType::COMMA},
                              {".", TokenType::DOT},
                              {":", TokenType::COLON},
                              {";", TokenType::SEMICOLON},
                              {"@", TokenType::AT}};
Lexer::Lexer(const std::string &source) : source(source) {}
std::vector<Token> Lexer::tokenize() {
  first_pass();
  check_for_keywords();
  return tokens;
}

bool Lexer::check_word_boundary(char c) {
  if (std::isalnum(c) || c == '_') {
    return true;
  }
  return false;
}

bool Lexer::is_punctuation(char c) {
  return c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
         c == ',' || c == '.' || c == ':' || c == ';' || c == '@';
}

void Lexer::first_pass() {
  auto push = [&](TokenType t, std::size_t beg, std::size_t end) {
    tokens.push_back({t, source.substr(beg, end - beg)});
  };

  std::size_t i = 0;
  const std::size_t n = source.size();

  while (i < n) {
    char c = source[i];

    if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
      continue;
    }

    // raw string
    if (i + 1 < n && (c == 'r' || (i + 2 < n && (c == 'b' || c == 'c') &&
                                   source[i + 1] == 'r'))) {
      TokenType token_type = TokenType::STRING_LITERAL;
      size_t prefix_len = 1;

      if (c == 'b') {
        token_type = TokenType::BYTE_STRING_LITERAL;
        prefix_len = 2;
      } else if (c == 'c') {
        token_type = TokenType::C_STRING_LITERAL;
        prefix_len = 2;
      }

      size_t hash_count = 0;
      size_t pos = i + prefix_len;

      while (pos < n && source[pos] == '#') {
        ++hash_count;
        ++pos;
      }

      if (pos < n && source[pos] == '"') {
        std::size_t beg = i;
        i = pos + 1;

        while (i < n) {
          if (source[i] == '"') {
            bool closing_match = true;
            for (size_t j = 0; j < hash_count; ++j) {
              if (i + 1 + j >= n || source[i + 1 + j] != '#') {
                closing_match = false;
                break;
              }
            }

            if (closing_match) {
              i = i + 1 + hash_count;
              push(token_type, beg, i);
              break;
            }
          }
          ++i;
        }
        continue;
      }
    }

    if (i + 1 < n && (c == 'b' || c == 'c')) {
      char next = source[i + 1];

      if (c == 'b' && next == '\'') {
        std::size_t beg = i;
        i += 2;
        bool escaped = false;
        while (i < n) {
          char d = source[i++];
          if (!escaped && d == '\'')
            break;
          escaped = (!escaped && d == '\\');
        }
        push(TokenType::BYTE_LITERAL, beg, i);
        continue;
      }

      if (c == 'b' && next == '"') {
        std::size_t beg = i;
        i += 2;
        bool escaped = false;
        while (i < n) {
          char d = source[i++];
          if (!escaped && d == '"')
            break;
          escaped = (!escaped && d == '\\');
        }
        push(TokenType::BYTE_STRING_LITERAL, beg, i);
        continue;
      }

      if (c == 'c' && next == '"') {
        std::size_t beg = i;
        i += 2;
        bool escaped = false;
        while (i < n) {
          char d = source[i++];
          if (!escaped && d == '"')
            break;
          escaped = (!escaped && d == '\\');
        }
        push(TokenType::C_STRING_LITERAL, beg, i);
        continue;
      }
    }

    // string literal
    if (c == '"') {
      std::size_t beg = i++;
      bool escaped = false;
      while (i < n) {
        char d = source[i++];
        if (!escaped && d == '"')
          break;
        escaped = (!escaped && d == '\\');
      }
      push(TokenType::STRING_LITERAL, beg, i);
      continue;
    }

    // char literal
    if (c == '\'') {
      std::size_t beg = i++;
      bool escaped = false;
      while (i < n) {
        char d = source[i++];
        if (!escaped && d == '\'')
          break;
        escaped = (!escaped && d == '\\');
      }
      push(TokenType::CHAR_LITERAL, beg, i);
      continue;
    }

    // integer literal
    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::size_t beg = i++;
      while (i < n && check_word_boundary(source[i]))
        ++i;
      push(TokenType::INTEGER_LITERAL, beg, i);
      continue;
    }

    // identifier or keyword
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::size_t beg = i++;
      while (i < n && check_word_boundary(source[i]))
        ++i;
      push(TokenType::NON_KEYWORD_IDENTIFIER, beg, i);
      continue;
    }

    // punctuation
    if (is_punctuation(c)) {
      auto it = punctuation_to_token_type.find(std::string_view(&source[i], 1));
      // check for two-character punctuation
      if (i + 1 < n && is_punctuation(source[i + 1])) {
        std::string_view punct(&source[i], 2);
        auto it2 = punctuation_to_token_type.find(punct);
        if (it2 != punctuation_to_token_type.end()) {
          push(it2->second, i, i + 2);
          i += 2;
          continue;
        }
      }
      push(it != punctuation_to_token_type.end() ? it->second : TokenType::UNKNOWN,
           i, i + 1);
      ++i;
      continue;
    }

    // operators
    bool matched = false;
    for (int len = 3; len >= 1 && !matched; --len) {
      if (i + len > n)
        continue;
      std::string_view op(&source[i], static_cast<size_t>(len));
      auto it = operator_to_token_type.find(op);
      if (it != operator_to_token_type.end()) {
        push(it->second, i, i + len);
        i += len;
        matched = true;
      }
    }
    if (matched)
      continue;

    // unknown token
    push(TokenType::UNKNOWN, i, i + 1);
    ++i;
  }
  push(TokenType::TOK_EOF, n, n);
}

void Lexer::check_for_keywords() {
  for (auto &tok : tokens) {
    if (tok.type == TokenType::NON_KEYWORD_IDENTIFIER) {
      auto it = keyword_to_token_type.find(tok.lexeme);
      if (it != keyword_to_token_type.end())
        tok.type = it->second;
    }
  }
}

} // namespace rc
