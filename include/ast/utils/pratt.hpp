#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast/nodes/expr.hpp"
#include "ast/utils/parsec.hpp"
#include "lexer/lexer.hpp"

namespace rc {
class Parser;
}

namespace pratt {

using ExprPtr = std::shared_ptr<rc::Expression>;

struct OpKey {
  rc::TokenType type;
  bool operator==(const OpKey &o) const noexcept { return type == o.type; }
};

struct OpKeyHash {
  size_t operator()(const OpKey &k) const noexcept {
    return std::hash<int>()(static_cast<int>(k.type));
  }
};

class PrattTable {
public:
  using NudFn =
      std::function<ExprPtr(const std::vector<rc::Token> &, size_t &)>;

  using Bp = std::pair<int, int>;
  struct LedEntry {
    Bp bp;
    std::function<ExprPtr(ExprPtr, const rc::Token &,
                          const std::vector<rc::Token> &, size_t &)>
        led;
  };

  void prefix(rc::TokenType t, NudFn nud) { prefix_[t] = std::move(nud); }

  void infix_left(rc::TokenType t, int precedence,
                  std::function<ExprPtr(ExprPtr, rc::Token, ExprPtr)> build) {
    LedEntry e;
    e.bp = {precedence, precedence + 1};
    infix_[OpKey{t}] =
        LedEntry{e.bp,
                 [this, build, &e](ExprPtr left, const rc::Token &op,
                                   const std::vector<rc::Token> &toks,
                                   size_t &pos) -> ExprPtr {
                   ExprPtr right = parse_expression(toks, pos, e.bp.second);
                   if (!right)
                     return nullptr;
                   return build(std::move(left), op, std::move(right));
                 }};
  }

  void infix_right(rc::TokenType t, int precedence,
                   std::function<ExprPtr(ExprPtr, rc::Token, ExprPtr)> build) {
    LedEntry e;
    e.bp = {precedence, precedence};
    infix_[OpKey{t}] =
        LedEntry{e.bp,
                 [this, build, &e](ExprPtr left, const rc::Token &op,
                                   const std::vector<rc::Token> &toks,
                                   size_t &pos) -> ExprPtr {
                   ExprPtr right = parse_expression(toks, pos, e.bp.second);
                   if (!right)
                     return nullptr;
                   return build(std::move(left), op, std::move(right));
                 }};
  }

  // Custom infix w/ explicit lbp/rbp
  void
  infix_custom(rc::TokenType t, int lbp, int rbp,
               std::function<ExprPtr(ExprPtr, const rc::Token &,
                                     const std::vector<rc::Token> &, size_t &)>
                   led) {
    infix_[OpKey{t}] = LedEntry{Bp{lbp, rbp}, std::move(led)};
  }

  ExprPtr parse_expression(const std::vector<rc::Token> &toks, size_t &pos,
                           int min_bp = 0) const {
    if (pos >= toks.size())
      return nullptr;
    rc::Token t = toks[pos++];
    auto it_nud = prefix_.find(t.type);
    if (it_nud == prefix_.end())
      return nullptr;
    ExprPtr left = it_nud->second(toks, pos);
    if (!left)
      return nullptr;

    for (;;) {
      if (pos >= toks.size())
        break;
      const rc::Token &look = toks[pos];

      const LedEntry *led = nullptr;

      auto it = infix_.find(OpKey{look.type});
      if (it != infix_.end())
        led = &it->second;

      if (!led)
        break;

      auto [lbp, rbp] = led->bp;
      if (lbp < min_bp)
        break;

      size_t op_pos = pos;
      rc::Token op = toks[pos++];

      ExprPtr new_left = led->led(left, op, toks, pos);
      if (!new_left) {
        pos = op_pos;
        break;
      }
      left = std::move(new_left);
    }

    return left;
  }

private:
  std::unordered_map<rc::TokenType, NudFn> prefix_;
  std::unordered_map<OpKey, LedEntry, OpKeyHash> infix_;
};

PrattTable default_table(rc::Parser *p);

inline parsec::Parser<ExprPtr> pratt_expr(const PrattTable &tbl,
                                          int min_bp = 0) {
  return parsec::Parser<ExprPtr>(
      [tbl, min_bp](const std::vector<rc::Token> &toks,
                    size_t &pos) -> parsec::ParseResult<ExprPtr> {
        size_t saved = pos;
        ExprPtr e = tbl.parse_expression(toks, pos, min_bp);
        if (!e) {
          pos = saved;
          return std::nullopt;
        }
        return e;
      });
}

inline auto validate_int_literal(const std::string &lexeme)
    -> std::tuple<long long, bool, std::string> { // value, has_suffix, suffix
  static const std::vector<std::string> valid_suffixes = {"i32", "u32", "isize",
                                                          "usize"};

  if (lexeme.empty()) {
    throw std::invalid_argument("Empty input string");
  }

  size_t start = 0;
  int base = 10;

  if (lexeme[0] == '-' || lexeme[0] == '+') {
    start = 1;
  }

  if (start + 1 < lexeme.length() && lexeme[start] == '0') {
    if (std::tolower(lexeme[start + 1]) == 'x') {
      base = 16;
      start += 2;
    } else if (std::tolower(lexeme[start + 1]) == 'b') {
      base = 2;
      start += 2;
    }
  }

  size_t suffix_start = lexeme.length();
  for (size_t i = start; i < lexeme.length(); ++i) {
    char c = std::tolower(lexeme[i]);
    bool valid_char = false;

    if (c == '_') {
      valid_char = true;
    } else if (base == 10 && std::isdigit(c)) {
      valid_char = true;
    } else if (base == 16 && (std::isdigit(c) || (c >= 'a' && c <= 'f'))) {
      valid_char = true;
    } else if (base == 2 && (c == '0' || c == '1')) {
      valid_char = true;
    }

    if (!valid_char) {
      suffix_start = i;
      break;
    }
  }

  std::string numeric_part = lexeme.substr(0, suffix_start);
  std::string clean_numeric = numeric_part;
  clean_numeric.erase(
      std::remove(clean_numeric.begin(), clean_numeric.end(), '_'),
      clean_numeric.end());

  long long value;
  try {
    value = std::stoll(clean_numeric, nullptr, base);
  } catch (const std::exception &e) {
    throw std::invalid_argument("Invalid numeric value: " + lexeme);
  }

  std::tuple<long long, bool, std::string> result;
  std::get<0>(result) = value;
  std::get<1>(result) = (suffix_start < lexeme.length());

  if (std::get<1>(result)) {
    std::get<2>(result) = lexeme.substr(suffix_start);

    if (std::find(valid_suffixes.begin(), valid_suffixes.end(),
                  std::get<2>(result)) == valid_suffixes.end()) {
      throw std::invalid_argument("Invalid suffix: " + std::get<2>(result));
    }

    if (std::get<2>(result) == "i32") {
      if (value < std::numeric_limits<int32_t>::min() ||
          value > std::numeric_limits<int32_t>::max()) {
        throw std::out_of_range("Value out of range for i32: " +
                                std::to_string(value));
      }
    } else if (std::get<2>(result) == "u32") {
      if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
        throw std::out_of_range("Value out of range for u32: " +
                                std::to_string(value));
      }
    } else if (std::get<2>(result) == "isize") {
      if (value < std::numeric_limits<intptr_t>::min() ||
          value > std::numeric_limits<intptr_t>::max()) {
        throw std::out_of_range("Value out of range for isize: " +
                                std::to_string(value));
      }
    } else if (std::get<2>(result) == "usize") {
      if (value < 0 || static_cast<unsigned long long>(value) >
                           std::numeric_limits<uintptr_t>::max()) {
        throw std::out_of_range("Value out of range for usize: " +
                                std::to_string(value));
      }
    }
  }

  return result;
}

} // namespace pratt