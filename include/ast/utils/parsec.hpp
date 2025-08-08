#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>

namespace parsec {

template <typename T> class Parser;

template <typename T> using ParseResult = std::optional<T>;

template <typename T>
using ParseFunction =
    std::function<ParseResult<T>(const std::string &, size_t &)>;

template <typename T> class Parser {
private:
  ParseFunction<T> parse_fn_;

public:
  Parser() = default;
  Parser(ParseFunction<T> fn) : parse_fn_(std::move(fn)) {}

  ParseResult<T> parse(const std::string &str, size_t &pos) const {
    return parse_fn_(str, pos);
  }

  T parse_or_throw(const std::string &str) const {
    size_t pos = 0;
    auto result = parse(str, pos);
    if (result) {
      return *result;
    }
    throw std::runtime_error("Parse error at position " + std::to_string(pos));
  }

  template <typename F>
  auto map(F transform) const
      -> Parser<decltype(transform(std::declval<T>()))> {
    using ResultType = decltype(transform(std::declval<T>()));

    return Parser<ResultType>(
        [*this, transform](const std::string &str,
                           size_t &pos) -> ParseResult<ResultType> {
          auto result = this->parse(str, pos);
          if (result) {
            return transform(*result);
          }
          return std::nullopt;
        });
  }

  template <typename U>
  Parser<std::variant<T, U>> operator|(const Parser<U> &other) const {
    return Parser<std::variant<T, U>>(
        [*this, other](const std::string &str,
                       size_t &pos) -> ParseResult<std::variant<T, U>> {
          size_t saved_pos = pos;

          auto result1 = this->parse(str, pos);
          if (result1) {
            return std::variant<T, U>(*result1);
          }

          pos = saved_pos;
          auto result2 = other.parse(str, pos);
          if (result2) {
            return std::variant<T, U>(*result2);
          }

          return std::nullopt;
        });
  }

  template <typename U>
  Parser<std::tuple<T, U>> operator+(const Parser<U> &other) const {
    return Parser<std::tuple<T, U>>(
        [*this, other](const std::string &str,
                       size_t &pos) -> ParseResult<std::tuple<T, U>> {
          size_t saved_pos = pos;

          auto result1 = this->parse(str, pos);
          if (!result1) {
            pos = saved_pos;
            return std::nullopt;
          }

          auto result2 = other.parse(str, pos);
          if (!result2) {
            pos = saved_pos;
            return std::nullopt;
          }

          return std::make_tuple(*result1, *result2);
        });
  }

  template <typename F> auto operator>>(F transform) const {
    return map(transform);
  }
};

class Token {
public:
  static Parser<char> satisfy(std::function<bool(char)> condition) {
    return Parser<char>(
        [condition](const std::string &str, size_t &pos) -> ParseResult<char> {
          if (pos < str.size() && condition(str[pos])) {
            return str[pos++];
          }
          return std::nullopt;
        });
  }

  static Parser<char> char_(char c) {
    return satisfy([c](char ch) { return ch == c; });
  }

  static Parser<std::string> string(const std::string &target) {
    return Parser<std::string>(
        [target](const std::string &str,
                 size_t &pos) -> ParseResult<std::string> {
          if (pos + target.size() <= str.size() &&
              str.substr(pos, target.size()) == target) {
            pos += target.size();
            return target;
          }
          return std::nullopt;
        });
  }

  // Always succeed with given value
  template <typename T> static Parser<T> pure(T value) {
    return Parser<T>([value](const std::string &, size_t &) -> ParseResult<T> {
      return value;
    });
  }

  // Always succeed with computed value
  template <typename F>
  static auto pure(F compute) -> Parser<decltype(compute())> {
    using ResultType = decltype(compute());
    return Parser<ResultType>(
        [compute](const std::string &, size_t &) -> ParseResult<ResultType> {
          return compute();
        });
  }
};

inline Parser<char> operator""_c(char c) { return Token::char_(c); }

inline Parser<std::string> operator""_s(const char *str, size_t) {
  return Token::string(std::string(str));
}

template <typename T> Parser<std::vector<T>> many(const Parser<T> &parser) {
  return Parser<std::vector<T>>(
      [parser](const std::string &str,
               size_t &pos) -> ParseResult<std::vector<T>> {
        std::vector<T> results;
        while (true) {
          auto result = parser.parse(str, pos);
          if (result) {
            results.push_back(*result);
          } else {
            break;
          }
        }
        return results;
      });
}

template <typename T> Parser<std::vector<T>> many1(const Parser<T> &parser) {
  return Parser<std::vector<T>>(
      [parser](const std::string &str,
               size_t &pos) -> ParseResult<std::vector<T>> {
        std::vector<T> results;
        auto first = parser.parse(str, pos);
        if (!first) {
          return std::nullopt;
        }
        results.push_back(*first);

        while (true) {
          auto result = parser.parse(str, pos);
          if (result) {
            results.push_back(*result);
          } else {
            break;
          }
        }
        return results;
      });
}

template <typename T>
Parser<std::optional<T>> optional(const Parser<T> &parser) {
  return Parser<std::optional<T>>(
      [parser](const std::string &str,
               size_t &pos) -> ParseResult<std::optional<T>> {
        auto result = parser.parse(str, pos);
        if (result) {
          return std::optional<T>(*result);
        }
        return std::optional<T>(std::nullopt);
      });
}

} // namespace parsec