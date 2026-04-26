#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ast/types.hpp"
#include "lexer/lexer.hpp"

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

  template <typename U> auto operator|(const Parser<U> &other) const {
    if constexpr (std::is_same_v<T, U>) {
      return Parser<T>([*this, other](const std::vector<rc::Token> &toks,
                                      size_t &pos) -> ParseResult<T> {
        size_t saved_pos = pos;
        if (auto result1 = this->parse(toks, pos)) {
          return result1;
        }
        pos = saved_pos;
        return other.parse(toks, pos);
      });
    } else {
      return Parser<std::variant<T, U>>(
          [*this, other](const std::vector<rc::Token> &toks,
                         size_t &pos) -> ParseResult<std::variant<T, U>> {
            size_t saved_pos = pos;
            if (auto result1 = this->parse(toks, pos)) {
              return std::variant<T, U>(std::in_place_type<T>, *result1);
            }
            pos = saved_pos;
            if (auto result2 = other.parse(toks, pos)) {
              return std::variant<T, U>(std::in_place_type<U>, *result2);
            }
            return std::nullopt;
          });
    }
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

  template <typename U> Parser<T> then_l(const Parser<U> &other) const {
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

  template <typename U> Parser<U> then_r(const Parser<U> &other) const {
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

rc::TokenType token_type_from_name(const std::string &name);

Parser<rc::Token> tok(rc::TokenType t);

Parser<rc::Token> tok(rc::TokenType t, std::string lexeme);

extern Parser<std::string> identifier;

extern Parser<std::string> int_literal;

using ExprParseFn = std::function<ParseResult<std::shared_ptr<rc::Expression>>(
    const std::vector<rc::Token> &, size_t &)>;

ParseResult<rc::AstType>
parse_type_impl(const std::vector<rc::Token> &toks, size_t &pos,
                const ExprParseFn &parse_expr);

// Build a type parser with an injected expression parser (for array sizes).
Parser<rc::AstType> typ_with_expr(ExprParseFn parse_expr);

} // namespace parsec
