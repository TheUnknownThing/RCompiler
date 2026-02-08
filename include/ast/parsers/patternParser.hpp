#pragma once

#include "ast/nodes/pattern.hpp"
#include "ast/utils/parsec.hpp"
#include "lexer/lexer.hpp"
#include <memory>

namespace rc {

class PatternParser {
public:
  PatternParser() {
    auto lazy_pattern_no_top_alt = [this](const auto &t, auto &p) {
      return pattern_no_top_alt().parse(t, p);
    };

    p_literal_ = parsec::int_literal.map(
        [](const std::string &s) -> std::shared_ptr<BasePattern> {
          return std::make_shared<LiteralPattern>(s, false);
        });

    p_identifier_ =
        parsec::optional(parsec::tok(rc::TokenType::REF))
            .combine(parsec::optional(parsec::tok(rc::TokenType::MUT)),
                     [](auto r, auto m) {
                       return std::make_pair(r.has_value(), m.has_value());
                     })
            .combine(parsec::identifier,
                     [](auto flags, auto id) -> std::shared_ptr<BasePattern> {
                       return std::make_shared<IdentifierPattern>(
                           id, flags.first, flags.second);
                     });

    p_reference_ =
        (parsec::tok(rc::TokenType::AMPERSAND) | parsec::tok(rc::TokenType::AND))
            .combine(parsec::optional(parsec::tok(rc::TokenType::MUT)),
                     [](auto, auto m) { return m.has_value(); })
            .combine(parsec::Parser<std::shared_ptr<BasePattern>>(
                         lazy_pattern_no_top_alt),
                     [](bool is_mut, auto sub) -> std::shared_ptr<BasePattern> {
                       return std::make_shared<ReferencePattern>(sub, is_mut);
                     });
  }

  parsec::ParseResult<std::shared_ptr<BasePattern>>
  parse(const std::vector<rc::Token> &toks, size_t &pos) {
    return or_pattern().parse(toks, pos);
  }

  parsec::Parser<std::shared_ptr<BasePattern>> p_literal_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_identifier_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_reference_;

  parsec::Parser<std::shared_ptr<BasePattern>> pattern_without_range() {
    return parsec::Parser<std::shared_ptr<BasePattern>>(
        [this](const std::vector<rc::Token> &toks, size_t &pos)
            -> parsec::ParseResult<std::shared_ptr<BasePattern>> {
          size_t saved = pos;

          auto parsers = {p_literal_, p_identifier_, p_reference_};

          for (const auto &parser : parsers) {
            auto result = parser.parse(toks, pos);
            if (result) {
              return result;
            }
            pos = saved;
          }

          return std::nullopt;
        });
  }

  parsec::Parser<std::shared_ptr<BasePattern>> pattern_no_top_alt() {
    return pattern_without_range();
  }

  parsec::Parser<std::shared_ptr<BasePattern>> or_pattern() {
    return pattern_no_top_alt().combine(
        parsec::many(parsec::tok(rc::TokenType::PIPE).thenR(pattern_no_top_alt())),
        [](auto first, auto rest) -> std::shared_ptr<BasePattern> {
          if (rest.empty()) {
            return first;
          }
          rest.insert(rest.begin(), first);
          return std::make_shared<OrPattern>(std::move(rest));
        });
  }

  parsec::Parser<std::shared_ptr<BasePattern>> any_pattern() {
    return or_pattern();
  }
};
} // namespace rc