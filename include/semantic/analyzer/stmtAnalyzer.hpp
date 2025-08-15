#pragma once

#include "ast/nodes/stmt.hpp"
#include "semantic/analyzer/symbolTable.hpp"

#include <memory>

namespace rc {

class SemanticContext;

class StmtAnalyzer : public BaseVisitor {
public:
  explicit StmtAnalyzer(SemanticContext &ctx);

  bool analyze(const std::shared_ptr<Statement> &stmt);

  void visit(BaseNode &node) override;

  void visit(BlockStatement &) override;
  void visit(LetStatement &) override;
  void visit(ExpressionStatement &) override;
  void visit(EmptyStatement &) override;

private:
  SemanticContext &ctx_;
};

} // namespace rc
