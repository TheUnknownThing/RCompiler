#include "ast/utils/pratt.hpp"

#include <limits>

namespace pratt {

parsec::Parser<ExprPtr> pratt_expr(const PrattTable &tbl, int min_bp) {
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

auto validate_int_literal(const std::string &lexeme)
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
