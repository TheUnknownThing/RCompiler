#pragma once
#include "../../lexer/lexer.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "parsec.hpp"

namespace pratt {

struct Expr {
  virtual ~Expr() = default;
};
using ExprPtr = std::shared_ptr<Expr>;

struct NameExpr : Expr {
  std::string name;
  explicit NameExpr(std::string n) : name(std::move(n)) {}
};
struct IntExpr : Expr {
  std::string value;
  explicit IntExpr(std::string v) : value(std::move(v)) {}
};

struct PrefixExpr : Expr {
  rc::Token op;
  ExprPtr right;
  PrefixExpr(rc::Token op, ExprPtr right)
      : op(std::move(op)), right(std::move(right)) {}
};

struct BinaryExpr : Expr {
  ExprPtr left;
  rc::Token op;
  ExprPtr right;
  BinaryExpr(ExprPtr l, rc::Token op, ExprPtr r)
      : left(std::move(l)), op(std::move(op)), right(std::move(r)) {}
};

struct GroupExpr : Expr {
  ExprPtr inner;
  explicit GroupExpr(ExprPtr e) : inner(std::move(e)) {}
};

struct OpKey {
  rc::TokenType type;
  std::string lex;
  bool operator==(const OpKey &o) const noexcept {
    return type == o.type && lex == o.lex;
  }
};

struct OpKeyHash {
  size_t operator()(const OpKey &k) const noexcept {
    return std::hash<int>()(static_cast<int>(k.type)) ^
           (std::hash<std::string>()(k.lex) << 1);
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
    infix_left(t, std::string{}, precedence, std::move(build));
  }
  void infix_left(rc::TokenType t, std::string lex, int precedence,
                  std::function<ExprPtr(ExprPtr, rc::Token, ExprPtr)> build) {
    LedEntry e;
    e.bp = {precedence, precedence + 1};
    infix_[OpKey{t, std::move(lex)}] =
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
    infix_right(t, std::string{}, precedence, std::move(build));
  }
  void infix_right(rc::TokenType t, std::string lex, int precedence,
                   std::function<ExprPtr(ExprPtr, rc::Token, ExprPtr)> build) {
    LedEntry e;
    e.bp = {precedence, precedence};
    infix_[OpKey{t, std::move(lex)}] =
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

  ExprPtr parse_expression(const std::vector<rc::Token> &toks, size_t &pos,
                           int min_bp = 0) const {
    if (pos >= toks.size())
      return nullptr;
    rc::Token t = toks[pos++];
    auto it_nud = prefix_.find(t.type);
    if (it_nud == prefix_.end())
      return nullptr;
    ExprPtr left =
        it_nud->second(toks, pos_for_nud_from_consumed(t, toks, pos));
    if (!left)
      return nullptr;

    for (;;) {
      if (pos >= toks.size())
        break;
      const rc::Token &look = toks[pos];

      const LedEntry *led = nullptr;
      auto it_exact = infix_.find(OpKey{look.type, look.lexeme});
      if (it_exact != infix_.end())
        led = &it_exact->second;
      else {
        auto it_any = infix_.find(OpKey{look.type, std::string{}});
        if (it_any != infix_.end())
          led = &it_any->second;
      }
      if (!led)
        break;

      auto [lbp, rbp] = led->bp;
      if (lbp < min_bp)
        break;

      rc::Token op = toks[pos++];
      ExprPtr new_left = led->led(std::move(left), op, toks, pos);
      if (!new_left)
        return nullptr;
      left = std::move(new_left);
    }

    return left;
  }

private:
  static size_t &pos_for_nud_from_consumed(const rc::Token &,
                                           const std::vector<rc::Token> &,
                                           size_t &pos) {
    return pos;
  }

  std::unordered_map<rc::TokenType, NudFn> prefix_;
  std::unordered_map<OpKey, LedEntry, OpKeyHash> infix_;
};

inline PrattTable default_table() {
  PrattTable tbl;

  tbl.prefix(rc::TokenType::NON_KEYWORD_IDENTIFIER,
             [](const std::vector<rc::Token> &toks, size_t &pos) -> ExprPtr {
               const rc::Token &prev = toks[pos - 1];
               return std::make_shared<NameExpr>(prev.lexeme);
             });
  tbl.prefix(rc::TokenType::INTEGER_LITERAL,
             [](const std::vector<rc::Token> &toks, size_t &pos) -> ExprPtr {
               const rc::Token &prev = toks[pos - 1];
               return std::make_shared<IntExpr>(prev.lexeme);
             });

  // ( expr )
  tbl.prefix(
      rc::TokenType::L_PAREN,
      [&tbl](const std::vector<rc::Token> &toks, size_t &pos) -> ExprPtr {
        ExprPtr inner = tbl.parse_expression(toks, pos, 0);
        if (!inner)
          return nullptr;
        if (pos >= toks.size() || toks[pos].type != rc::TokenType::R_PAREN)
          return nullptr;
        ++pos;
        return std::make_shared<GroupExpr>(std::move(inner));
      });

  // prefix ops: + - !
  tbl.prefix(
      rc::TokenType::PLUS,
      [&tbl](const std::vector<rc::Token> &toks, size_t &pos) -> ExprPtr {
        rc::Token op = toks[pos - 1];
        ExprPtr right = tbl.parse_expression(toks, pos, 100);
        if (!right)
          return nullptr;
        return std::make_shared<PrefixExpr>(op, std::move(right));
      });
  tbl.prefix(
      rc::TokenType::MINUS,
      [&tbl](const std::vector<rc::Token> &toks, size_t &pos) -> ExprPtr {
        rc::Token op = toks[pos - 1];
        ExprPtr right = tbl.parse_expression(toks, pos, 100);
        if (!right)
          return nullptr;
        return std::make_shared<PrefixExpr>(op, std::move(right));
      });
  tbl.prefix(
      rc::TokenType::NOT,
      [&tbl](const std::vector<rc::Token> &toks, size_t &pos) -> ExprPtr {
        rc::Token op = toks[pos - 1];
        ExprPtr right = tbl.parse_expression(toks, pos, 100);
        if (!right)
          return nullptr;
        return std::make_shared<PrefixExpr>(op, std::move(right));
      });

  auto bin = [](ExprPtr l, rc::Token op, ExprPtr r) {
    return std::make_shared<BinaryExpr>(std::move(l), std::move(op),
                                        std::move(r));
  };

  // 70: * / %
  tbl.infix_left(rc::TokenType::STAR, 70, bin);
  tbl.infix_left(rc::TokenType::SLASH, 70, bin);
  tbl.infix_left(rc::TokenType::PERCENT, 70, bin);
  // 60: + -
  tbl.infix_left(rc::TokenType::PLUS, 60, bin);
  tbl.infix_left(rc::TokenType::MINUS, 60, bin);
  // 50: << >>
  tbl.infix_left(rc::TokenType::SHL, 50, bin);
  tbl.infix_left(rc::TokenType::SHR, 50, bin);
  // 40: & (bitwise)
  tbl.infix_left(rc::TokenType::AMPERSAND, 40, bin);
  // 35: ^
  tbl.infix_left(rc::TokenType::CARET, 35, bin);
  // 30: |
  tbl.infix_left(rc::TokenType::PIPE, 30, bin);
  // 20: < > <= >=
  tbl.infix_left(rc::TokenType::LT, 20, bin);
  tbl.infix_left(rc::TokenType::GT, 20, bin);
  tbl.infix_left(rc::TokenType::LE, 20, bin);
  tbl.infix_left(rc::TokenType::GE, 20, bin);
  // 15: == !=
  tbl.infix_left(rc::TokenType::NE, 15, bin);
  tbl.infix_left(rc::TokenType::EQ, "==", 15, bin);
  // 10: &&
  tbl.infix_left(rc::TokenType::AND, 10, bin);
  // 9: ||
  tbl.infix_left(rc::TokenType::OR, 9, bin);
  // 5: assignments
  tbl.infix_right(rc::TokenType::EQ, "=", 5, bin);
  tbl.infix_right(rc::TokenType::PLUS_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::MINUS_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::STAR_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::SLASH_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::PERCENT_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::AMPERSAND_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::PIPE_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::CARET_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::SHL_EQ, 5, bin);
  tbl.infix_right(rc::TokenType::SHR_EQ, 5, bin);

  return tbl;
}

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

} // namespace pratt