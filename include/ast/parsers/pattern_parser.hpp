#pragma once

#include "ast/nodes/pattern.hpp"
#include "ast/utils/parsec.hpp"
#include "lexer/lexer.hpp"

namespace rc {

using namespace parsec;

class PatternParser {
public:
  PatternParser() {
    auto lazy_any_pattern = [this](const auto &t, auto &p) {
      return any_pattern().parse(t, p);
    };
    auto lazy_pattern_no_top_alt = [this](const auto &t, auto &p) {
      return pattern_no_top_alt().parse(t, p);
    };

    p_path_ = identifier.combine(
        many(tok(rc::TokenType::COLON_COLON).thenR(identifier)),
        [](const auto &first, const auto &rest) {
          Path p = {first};
          p.insert(p.end(), rest.begin(), rest.end());
          return p;
        });

    p_wildcard_ =
        tok(rc::TokenType::NON_KEYWORD_IDENTIFIER) // '_' is an identifier
            .map([](auto) {
              return std::make_shared<BasePattern>(WildcardPattern{});
            });

    p_rest_ = tok(rc::TokenType::DOT_DOT).map([](auto) {
      return std::make_shared<BasePattern>(RestPattern{});
    });

    p_literal_ = int_literal.map([](const std::string &s) {
      return std::make_shared<BasePattern>(LiteralPattern{s, false});
    });

    p_identifier_ =
        optional(tok(rc::TokenType::REF))
            .combine(optional(tok(rc::TokenType::MUT)),
                     [](auto r, auto m) {
                       return std::make_pair(r.has_value(), m.has_value());
                     })
            .combine(identifier,
                     [](auto flags, auto id) {
                       return std::make_tuple(flags.first, flags.second, id);
                     })
            .combine(
                optional(
                    tok(rc::TokenType::AT)
                        .thenR(parsec::Parser<std::shared_ptr<BasePattern>>(
                            lazy_pattern_no_top_alt))),
                [](auto t, auto sub) {
                  return std::make_shared<BasePattern>(IdentifierPattern{
                      std::get<2>(t), std::get<0>(t), std::get<1>(t), sub});
                });

    p_grouped_ = tok(rc::TokenType::L_PAREN)
                     .thenR(parsec::Parser<std::shared_ptr<BasePattern>>(
                         lazy_any_pattern))
                     .thenL(tok(rc::TokenType::R_PAREN));

    auto comma_separated_patterns =
        parsec::Parser<std::vector<std::shared_ptr<BasePattern>>>(
            [lazy_any_pattern](auto &t, auto &p) {
              std::vector<std::shared_ptr<BasePattern>> patterns;
              auto first =
                  parsec::Parser<std::shared_ptr<BasePattern>>(lazy_any_pattern)
                      .parse(t, p);
              if (!first)
                return patterns; // empty list
              patterns.push_back(*first);

              while (true) {
                size_t saved = p;
                if (!tok(rc::TokenType::COMMA).parse(t, p)) {
                  p = saved;
                  break;
                }

                if (tok(rc::TokenType::R_PAREN).parse(t, p) ||
                    tok(rc::TokenType::R_BRACKET).parse(t, p)) {
                  p = saved;
                  break;
                }
                p = saved;

                auto next =
                    tok(rc::TokenType::COMMA)
                        .thenR(parsec::Parser<std::shared_ptr<BasePattern>>(
                            lazy_any_pattern))
                        .parse(t, p);
                if (!next)
                  throw std::runtime_error(
                      "Expected pattern after ',' in pattern list");

                patterns.push_back(*next);
              }
              return patterns;
            });

    p_tuple_ = tok(rc::TokenType::L_PAREN)
                   .thenR(comma_separated_patterns)
                   .thenL(tok(rc::TokenType::R_PAREN))
                   .map([](auto elems) {
                     return std::make_shared<BasePattern>(
                         TuplePattern{std::move(elems)});
                   });

    p_slice_ = tok(rc::TokenType::L_BRACKET)
                   .thenR(comma_separated_patterns)
                   .thenL(tok(rc::TokenType::R_BRACKET))
                   .map([](auto elems) {
                     return std::make_shared<BasePattern>(
                         SlicePattern{std::move(elems)});
                   });

    // FIX: ADD STRUCT PATTERN

    p_reference_ = (tok(rc::TokenType::AMPERSAND) | tok(rc::TokenType::AND))
                       .combine(optional(tok(rc::TokenType::MUT)),
                                [](auto, auto m) { return m.has_value(); })
                       .combine(parsec::Parser<std::shared_ptr<BasePattern>>(
                                    lazy_pattern_no_top_alt),
                                [](bool is_mut, auto sub) {
                                  return std::make_shared<BasePattern>(
                                      ReferencePattern{sub, is_mut});
                                });

    p_path_pattern_ = p_path_.map([](const Path &path) {
      return std::make_shared<BasePattern>(PathPattern{path});
    });
  }

  parsec::ParseResult<std::shared_ptr<BasePattern>>
  parse(const std::vector<rc::Token> &toks, size_t &pos) {
    return or_pattern().parse(toks, pos);
  }

  parsec::Parser<Path> p_path_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_wildcard_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_rest_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_literal_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_identifier_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_grouped_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_tuple_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_slice_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_struct_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_reference_;
  parsec::Parser<std::shared_ptr<BasePattern>> p_path_pattern_;

  parsec::Parser<std::shared_ptr<BasePattern>> pattern_without_range() {
    return parsec::Parser<std::shared_ptr<BasePattern>>(
        [this](const std::vector<rc::Token> &toks, size_t &pos)
            -> parsec::ParseResult<std::shared_ptr<BasePattern>> {
          size_t saved = pos;

          auto parsers = {p_wildcard_,    p_rest_,  p_literal_, p_identifier_,
                          p_grouped_,     p_tuple_, p_slice_,   p_reference_,
                          p_path_pattern_ /*, p_struct_ */};

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
        many(tok(rc::TokenType::PIPE).thenR(pattern_no_top_alt())),
        [](auto first, auto rest) {
          if (rest.empty()) {
            return first;
          }
          rest.insert(rest.begin(), first);
          return std::make_shared<BasePattern>(OrPattern{std::move(rest)});
        });
  }

  parsec::Parser<std::shared_ptr<BasePattern>> any_pattern() {
    return or_pattern();
  }
};
} // namespace rc