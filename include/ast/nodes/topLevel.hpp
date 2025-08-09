#pragma once

#include "../../lexer/lexer.hpp"
#include "../types.hpp"
#include "base.hpp"
#include "stmt.hpp"

#include <memory>
#include <vector>
#include <optional>

namespace rc {
class FunctionDecl : public BaseNode {
public:
  std::string name;
  std::optional<std::vector<std::pair<std::string, LiteralType>>> params;
  LiteralType return_type;

  FunctionDecl(const std::string &nameTok,
               const std::optional<std::vector<std::pair<std::string, LiteralType>>> &params,
               LiteralType return_type)
      : name(nameTok), params(params), return_type(return_type) {}

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