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
  void infix_custom(rc::TokenType t, int lbp, int rbp,
                    std::function<ExprPtr(ExprPtr, const rc::Token &,
                                           const std::vector<rc::Token> &,
                                           size_t &)> led) {
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

      rc::Token op = toks[pos++];
      ExprPtr new_left = led->led(std::move(left), op, toks, pos);
      if (!new_left)
        return nullptr;
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

} // namespace pratt