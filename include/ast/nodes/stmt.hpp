#pragma once

#include "base.hpp"

#include <map>
#include <string>
#include <vector>

namespace rc {

class Statement : public nc::BaseNode {
public:
  virtual void accept(nc::BaseVisitor &visitor) = 0;
};

class ExpressionStatement : public Statement {
public:
  nc::BaseNode *expression;

  ExpressionStatement(nc::BaseNode *expr) : expression(expr) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class BlockStatement : public Statement {
public:
  std::vector<Statement *> statements;

  BlockStatement(const std::vector<Statement *> &stmts) : statements(stmts) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class LetStatement : public Statement {
public:
  std::string identifier;
  nc::BaseNode *initializer;

  LetStatement(const std::string &id, nc::BaseNode *init)
      : identifier(id), initializer(init) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ReturnStatement : public Statement {
public:
  nc::BaseNode *expression;

  ReturnStatement(nc::BaseNode *expr) : expression(expr) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class IfStatement : public Statement {
public:
  nc::BaseNode *condition;
  Statement *thenBranch;
  Statement *elseBranch;
  
  IfStatement(nc::BaseNode *cond, Statement *thenBr, Statement *elseBr)
      : condition(cond), thenBranch(thenBr), elseBranch(elseBr) {}
    void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class WhileStatement : public Statement {
public:
  nc::BaseNode *condition;
  Statement *body;

  WhileStatement(nc::BaseNode *cond, Statement *bdy)
      : condition(cond), body(bdy) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc