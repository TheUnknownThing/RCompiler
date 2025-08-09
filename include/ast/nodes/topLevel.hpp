#pragma once

#include "../types.hpp"
#include "../../lexer/lexer.hpp"
#include "base.hpp"
#include "stmt.hpp"

#include <memory>
#include <vector>

namespace rc {
class FunctionDecl : public BaseNode {
public:
  Token name;
  std::vector<Token> parameters;
  std::vector<LiteralType> paramers_types;
  std::vector<LiteralType> return_types;
  BlockStatement *body;

  FunctionDecl(const Token &nameTok, const std::vector<Token> &params,
               const std::vector<LiteralType> &param_types,
               const std::vector<LiteralType> &return_types,
               BlockStatement *body)
      : name(nameTok), parameters(params), paramers_types(param_types),
        return_types(return_types), body(body) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class StructDecl : public BaseNode {
public:
  Token name;
  std::vector<std::pair<Token, Token>> fields; // field name, type token

  StructDecl(const Token &nameTok,
             const std::vector<std::pair<Token, Token>> &fields)
      : name(nameTok), fields(fields) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class RootNode : public BaseNode {
public:
  std::vector<std::unique_ptr<BaseNode>> children;

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc