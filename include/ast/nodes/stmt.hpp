#pragma once

#include "base.hpp"
#include "../../lexer/lexer.hpp"

#include <map>
#include <string>
#include <vector>

namespace rc {

class Statement : public BaseNode {
public:
  virtual void accept(BaseVisitor &visitor) = 0;
};

class BlockStatement : public Statement {
public:
  std::vector<Statement *> statements;

  BlockStatement(const std::vector<Statement *> &stmts) : statements(stmts) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class LetStatement : public Statement {
public:
  Token identifier;
  BaseNode *initializer;

  LetStatement(const Token &id, BaseNode *init)
      : identifier(id), initializer(init) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};


} // namespace rc