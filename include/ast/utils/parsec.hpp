#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "../../lexer/lexer.hpp"
#include "../types.hpp"

namespace parsec {

template <typename T> using ParseResult = std::optional<T>;

template <typename T>
using ParseFunction =
    std::function<ParseResult<T>(const std::vector<rc::Token> &, size_t &)>;

template <typename T> class Parser {
  ParseFunction<T> fn_;

public:
  Parser() = default;
  explicit Parser(ParseFunction<T> fn) : fn_(std::move(fn)) {}

  ParseResult<T> parse(const std::vector<rc::Token> &toks, size_t &pos) const {
    return fn_(toks, pos);
  }

  T parse_or_throw(const std::vector<rc::Token> &toks) const {
    size_t pos = 0;
    auto r = parse(toks, pos);
    if (r)
      return *r;
    throw std::runtime_error("Parse error at token index " +
                             std::to_string(pos));
  }

  template <typename F>
  auto map(F transform) const
      -> Parser<decltype(transform(std::declval<T>()))> {
    using ResultType = decltype(transform(std::declval<T>()));
    return Parser<ResultType>(
        [*this, transform](const std::vector<rc::Token> &toks,
                           size_t &pos) -> ParseResult<ResultType> {
          auto result = this->parse(toks, pos);
          if (!result)
            return std::nullopt;
          return transform(*result);
        });
  }

  template <typename U>
  Parser<std::variant<T, U>> operator|(const Parser<U> &other) const {
    return Parser<std::variant<T, U>>(
        [*this, other](const std::vector<rc::Token> &toks,
                       size_t &pos) -> ParseResult<std::variant<T, U>> {
          size_t saved_pos = pos;
          if (auto result1 = this->parse(toks, pos))
            return std::variant<T, U>(*result1);
          pos = saved_pos;
          if (auto result2 = other.parse(toks, pos))
            return std::variant<T, U>(*result2);
          return std::nullopt;
        });
  }

  template <typename U>
  Parser<std::tuple<T, U>> operator+(const Parser<U> &other) const {
    return Parser<std::tuple<T, U>>(
        [*this, other](const std::vector<rc::Token> &toks,
                       size_t &pos) -> ParseResult<std::tuple<T, U>> {
          size_t saved_pos = pos;
          auto result1 = this->parse(toks, pos);
          if (!result1) {
            pos = saved_pos;
            return std::nullopt;
          }
          auto result2 = other.parse(toks, pos);
          if (!result2) {
            pos = saved_pos;
            return std::nullopt;
          }
          return std::make_tuple(*result1, *result2);
        });
  }

  template <typename U> Parser<T> thenL(const Parser<U> &other) const {
    return Parser<T>([*this, other](const std::vector<rc::Token> &toks,
                                    size_t &pos) -> ParseResult<T> {
      size_t saved_pos = pos;
      auto result1 = this->parse(toks, pos);
      if (!result1) {
        pos = saved_pos;
        return std::nullopt;
      }
      auto result2 = other.parse(toks, pos);
      if (!result2) {
        pos = saved_pos;
        return std::nullopt;
      }
      return *result1;
    });
  }

  template <typename U> Parser<U> thenR(const Parser<U> &other) const {
    return Parser<U>([*this, other](const std::vector<rc::Token> &toks,
                                    size_t &pos) -> ParseResult<U> {
      size_t saved_pos = pos;
      auto result1 = this->parse(toks, pos);
      if (!result1) {
        pos = saved_pos;
        return std::nullopt;
      }
      auto result2 = other.parse(toks, pos);
      if (!result2) {
        pos = saved_pos;
        return std::nullopt;
      }
      return *result2;
    });
  }

  template <typename U, typename F, typename R = std::invoke_result_t<F, T, U>>
  Parser<R> combine(const Parser<U> &other, F f) const {
    return Parser<R>([*this, other, f](const std::vector<rc::Token> &toks,
                                       size_t &pos) -> ParseResult<R> {
      size_t saved_pos = pos;
      auto result1 = this->parse(toks, pos);
      if (!result1) {
        pos = saved_pos;
        return std::nullopt;
      }
      auto result2 = other.parse(toks, pos);
      if (!result2) {
        pos = saved_pos;
        return std::nullopt;
      }
      return f(*result1, *result2);
    });
  }

  template <typename F> auto operator>>(F transform) const {
    return map(transform);
  }
};

template <typename T> Parser<std::vector<T>> many(const Parser<T> &p) {
  return Parser<std::vector<T>>(
      [p](const std::vector<rc::Token> &toks,
          size_t &pos) -> ParseResult<std::vector<T>> {
        std::vector<T> out;
        for (;;) {
          size_t saved = pos;
          auto r = p.parse(toks, pos);
          if (!r) {
            pos = saved;
            break;
          }
          out.push_back(*r);
        }
        return out;
      });
}

template <typename T> Parser<std::vector<T>> many1(const Parser<T> &p) {
  return Parser<std::vector<T>>(
      [p](const std::vector<rc::Token> &toks,
          size_t &pos) -> ParseResult<std::vector<T>> {
        std::vector<T> out;
        auto first = p.parse(toks, pos);
        if (!first)
          return std::nullopt;
        out.push_back(*first);
        for (;;) {
          size_t saved_pos = pos;
          auto r = p.parse(toks, pos);
          if (!r) {
            pos = saved_pos;
            break;
          }
          out.push_back(*r);
        }
        return out;
      });
}

template <typename T> Parser<std::optional<T>> optional(const Parser<T> &p) {
  return Parser<std::optional<T>>(
      [p](const std::vector<rc::Token> &toks,
          size_t &pos) -> ParseResult<std::optional<T>> {
        size_t saved_pos = pos;
        auto r = p.parse(toks, pos);
        if (r)
          return std::optional<T>(*r);
        pos = saved_pos;
        return std::optional<T>(std::nullopt);
      });
}

inline rc::TokenType token_type_from_name(const std::string &name) {
  static const std::unordered_map<std::string, rc::TokenType> table = {
#define X(name, str) {#name, rc::TokenType::name},
#include "../../lexer/token_defs.def"
#undef X
  };
  auto it = table.find(name);
  if (it == table.end()) {
    throw std::runtime_error("Unknown token type name: " + name);
  }
  return it->second;
}

inline Parser<rc::Token> tok(rc::TokenType t) {
  return Parser<rc::Token>([t](const std::vector<rc::Token> &toks,
                               size_t &pos) -> ParseResult<rc::Token> {
    if (pos < toks.size() && toks[pos].type == t)
      return toks[pos++];
    return std::nullopt;
  });
}

inline Parser<std::string> identifier =
    Parser<std::string>([](const std::vector<rc::Token> &toks,
                           size_t &pos) -> ParseResult<std::string> {
      if (pos < toks.size() &&
          toks[pos].type == rc::TokenType::NON_KEYWORD_IDENTIFIER) {
        return toks[pos++].lexeme;
      }
      return std::nullopt;
    });

inline Parser<std::string> int_literal =
    Parser<std::string>([](const std::vector<rc::Token> &toks,
                           size_t &pos) -> ParseResult<std::string> {
      if (pos < toks.size() &&
          toks[pos].type == rc::TokenType::INTEGER_LITERAL) {
        return toks[pos++].lexeme;
      }
      return std::nullopt;
    });

inline Parser<rc::LiteralType> typ =
    Parser<rc::LiteralType>([](const std::vector<rc::Token> &toks,
                               size_t &pos) -> ParseResult<rc::LiteralType> {
      size_t saved_pos = pos;

      // [...]
      if (pos < toks.size() && toks[pos].type == rc::TokenType::L_BRACKET) {
        pos++;
        auto elem_ty = typ.parse(toks, pos);
        if (!elem_ty) {
          pos = saved_pos;
          return std::nullopt;
        }

        if (pos < toks.size() && toks[pos].type == rc::TokenType::SEMICOLON) {
          pos++;
          if (pos < toks.size() &&
              toks[pos].type == rc::TokenType::INTEGER_LITERAL) {
            std::string s = toks[pos++].lexeme;
            // strip known integer suffixes
            auto strip_suffix = [](std::string &str) {
              static const char *suffixes[] = {"i32", "isize", "u32", "usize"};
              for (auto suf : suffixes) {
                std::string suffix{suf};
                if (str.size() >= suffix.size() &&
                    str.compare(str.size() - suffix.size(), suffix.size(),
                                suffix) == 0) {
                  str.erase(str.size() - suffix.size());
                  break;
                }
              }
            };
            strip_suffix(s);
            s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
            int base = 10;
            if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
              base = 16;
              s.erase(0, 2);
            } else if (s.rfind("0o", 0) == 0 || s.rfind("0O", 0) == 0) {
              base = 8;
              s.erase(0, 2);
            } else if (s.rfind("0b", 0) == 0 || s.rfind("0B", 0) == 0) {
              base = 2;
              s.erase(0, 2);
            }
            std::uint64_t size_val = 0;
            try {
              size_val = s.empty() ? 0ULL
                                   : static_cast<std::uint64_t>(
                                         std::stoull(s, nullptr, base));
            } catch (...) {
              pos = saved_pos;
              return std::nullopt;
            }
            if (pos < toks.size() &&
                toks[pos].type == rc::TokenType::R_BRACKET) {
              pos++; // consume ']'
              return rc::LiteralType::array(*elem_ty, size_val);
            }
          }
          pos = saved_pos;
          return std::nullopt;
        } else if (pos < toks.size() &&
                   toks[pos].type == rc::TokenType::R_BRACKET) {
          // slice
          pos++;
          return rc::LiteralType::slice(*elem_ty);
        }
        pos = saved_pos;
      }

      // tuple type
      if (pos < toks.size() && toks[pos].type == rc::TokenType::L_PAREN) {
        pos++; // consume '('
        std::vector<rc::LiteralType> elements;

        // () as unit type
        if (pos < toks.size() && toks[pos].type == rc::TokenType::R_PAREN) {
          pos++;
          return rc::LiteralType::tuple(std::move(elements));
        }

        auto first = typ.parse(toks, pos);
        if (first) {
          elements.push_back(*first);

          while (pos < toks.size() && toks[pos].type == rc::TokenType::COMMA) {
            pos++; // consume ','
            auto next = typ.parse(toks, pos);
            if (!next) {
              pos = saved_pos;
              return std::nullopt;
            }
            elements.push_back(*next);
          }

          if (pos < toks.size() && toks[pos].type == rc::TokenType::R_PAREN) {
            pos++; // consume ')'
            return rc::LiteralType::tuple(std::move(elements));
          }
        }
        pos = saved_pos;
      }

      if (pos < toks.size() && toks[pos].type == rc::TokenType::NOT) {
        pos++;
        return rc::LiteralType::base(rc::PrimitiveLiteralType::NEVER);
      }

      // primitive type or path type
      if (pos < toks.size() &&
          toks[pos].type == rc::TokenType::NON_KEYWORD_IDENTIFIER) {
        std::vector<std::string> segments;
        segments.push_back(toks[pos++].lexeme);
        while (pos + 1 < toks.size() &&
               toks[pos].type == rc::TokenType::COLON_COLON &&
               toks[pos + 1].type == rc::TokenType::NON_KEYWORD_IDENTIFIER) {
          pos++;
          segments.push_back(toks[pos++].lexeme);
        }

        if (segments.size() == 1) {
          const std::string &name = segments.front();
          auto it = rc::literal_type_map.find(name);
          if (it != rc::literal_type_map.end()) {
            return it->second;
          }
        }
        return rc::LiteralType::path(std::move(segments));
      }

      return std::nullopt;
    });

} // namespace parsec