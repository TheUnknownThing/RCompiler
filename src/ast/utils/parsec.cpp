#include "ast/utils/parsec.hpp"

namespace parsec {

rc::TokenType token_type_from_name(const std::string &name) {
  static const std::unordered_map<std::string, rc::TokenType> table = {
#define X(name, str) {#name, rc::TokenType::name},
#include "lexer/token_defs.def"
#undef X
  };
  auto it = table.find(name);
  if (it == table.end()) {
    throw std::runtime_error("Unknown token type name: " + name);
  }
  return it->second;
}
Parser<rc::Token> tok(rc::TokenType t) {
  return Parser<rc::Token>([t](const std::vector<rc::Token> &toks,
                               size_t &pos) -> ParseResult<rc::Token> {
    if (pos < toks.size() && toks[pos].type == t)
      return toks[pos++];
    return std::nullopt;
  });
}
Parser<rc::Token> tok(rc::TokenType t, std::string lexeme) {
  return Parser<rc::Token>([t, lexeme](const std::vector<rc::Token> &toks,
                                       size_t &pos) -> ParseResult<rc::Token> {
    if (pos < toks.size() && toks[pos].type == t && toks[pos].lexeme == lexeme)
      return toks[pos++];
    return std::nullopt;
  });
}
Parser<std::string> identifier =
    Parser<std::string>([](const std::vector<rc::Token> &toks,
                           size_t &pos) -> ParseResult<std::string> {
      if (pos < toks.size() &&
          toks[pos].type == rc::TokenType::NON_KEYWORD_IDENTIFIER) {
        return toks[pos++].lexeme;
      }
      return std::nullopt;
    });
Parser<std::string> int_literal =
    Parser<std::string>([](const std::vector<rc::Token> &toks,
                           size_t &pos) -> ParseResult<std::string> {
      if (pos < toks.size() &&
          toks[pos].type == rc::TokenType::INTEGER_LITERAL) {
        return toks[pos++].lexeme;
      }
      return std::nullopt;
    });
ParseResult<rc::AstType>
parse_type_impl(const std::vector<rc::Token> &toks, size_t &pos,
                const ExprParseFn &parse_expr) {
  size_t saved_pos = pos;

  // Array or slice: [ T ; expr ] or [ T ]
  if (pos < toks.size() && toks[pos].type == rc::TokenType::L_BRACKET) {
    pos++; // consume '['
    auto elem_ty = parse_type_impl(toks, pos, parse_expr);
    if (!elem_ty) {
      pos = saved_pos;
      return std::nullopt;
    }

    // Array form: [ T ; expr ]
    if (pos < toks.size() && toks[pos].type == rc::TokenType::SEMICOLON) {
      pos++; // consume ';'
      auto size_expr = parse_expr
                           ? parse_expr(toks, pos)
                           : ParseResult<std::shared_ptr<rc::Expression>>{};
      if (!size_expr) {
        // failed to parse expression; backtrack entire [ T ; expr ... ]
        pos = saved_pos;
        return std::nullopt;
      }
      if (pos >= toks.size() || toks[pos].type != rc::TokenType::R_BRACKET) {
        pos = saved_pos;
        return std::nullopt;
      }
      pos++; // consume ']'
      return rc::AstType::array(std::move(*elem_ty), *size_expr);
    }

    // Slice form: [ T ]
    if (pos < toks.size() && toks[pos].type == rc::TokenType::R_BRACKET) {
      pos++; // consume ']'
      return rc::AstType::slice(std::move(*elem_ty));
    }

    pos = saved_pos;
  }

  // Tuple type
  if (pos < toks.size() && toks[pos].type == rc::TokenType::L_PAREN) {
    pos++; // consume '('
    std::vector<rc::AstType> elements;

    // () as unit type
    if (pos < toks.size() && toks[pos].type == rc::TokenType::R_PAREN) {
      pos++;
      return rc::AstType(rc::PrimitiveAstType::UNIT);
    }

    auto first = parse_type_impl(toks, pos, parse_expr);
    if (first) {
      elements.push_back(*first);

      while (pos < toks.size() && toks[pos].type == rc::TokenType::COMMA) {
        pos++; // consume ','
        auto next = parse_type_impl(toks, pos, parse_expr);
        if (!next) {
          pos = saved_pos;
          return std::nullopt;
        }
        elements.push_back(*next);
      }

      if (pos < toks.size() && toks[pos].type == rc::TokenType::R_PAREN) {
        pos++; // consume ')'
        return rc::AstType::tuple(std::move(elements));
      }
    }
    pos = saved_pos;
  }

  // Never type '!'
  if (pos < toks.size() && toks[pos].type == rc::TokenType::NOT) {
    pos++;
    return rc::AstType::base(rc::PrimitiveAstType::NEVER);
  }

  // Reference type: & [mut] T
  if (pos < toks.size() && toks[pos].type == rc::TokenType::AMPERSAND) {
    pos++;
    bool is_mutable = false;
    if (pos < toks.size() && toks[pos].type == rc::TokenType::MUT) {
      is_mutable = true;
      pos++;
    }
    auto target = parse_type_impl(toks, pos, parse_expr);
    if (!target) {
      pos = saved_pos;
      return std::nullopt;
    }
    return rc::AstType::reference(std::move(*target), is_mutable);
  }

  // Primitive type or path type
  if (pos < toks.size() &&
      toks[pos].type == rc::TokenType::NON_KEYWORD_IDENTIFIER) {
    std::vector<std::string> segments;
    segments.push_back(toks[pos++].lexeme);
    while (pos + 1 < toks.size() &&
           toks[pos].type == rc::TokenType::COLON_COLON &&
           toks[pos + 1].type == rc::TokenType::NON_KEYWORD_IDENTIFIER) {
      pos++; // consume '::'
      segments.push_back(toks[pos++].lexeme);
    }

    if (segments.size() == 1) {
      const std::string &name = segments.front();
      auto it = rc::literal_type_map.find(name);
      if (it != rc::literal_type_map.end()) {
        return it->second;
      }
    }
    return rc::AstType::path(std::move(segments));
  }

  if (pos < toks.size() && toks[pos].type == rc::TokenType::SELF_TYPE) {
    pos++;
    return rc::AstType::path(std::vector<std::string>{"Self"});
  }

  return std::nullopt;
}
Parser<rc::AstType> typ_with_expr(ExprParseFn parse_expr) {
  return Parser<rc::AstType>(
      [parse_expr](const std::vector<rc::Token> &toks,
                   size_t &pos) -> ParseResult<rc::AstType> {
        return parse_type_impl(toks, pos, parse_expr);
      });
}

} // namespace parsec
