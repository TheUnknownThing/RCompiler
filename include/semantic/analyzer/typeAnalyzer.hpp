#pragma once

#include "ast/nodes/base.hpp"
#include "ast/nodes/expr.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"

namespace rc {

class SemanticContext;

class TypeAnalyzer : public BaseVisitor {
public:
  explicit TypeAnalyzer(SemanticContext &ctx);

  void check(const std::shared_ptr<RootNode> &root);

  void visit(BaseNode &node) override;

  void visit(FunctionDecl &) override;
  void visit(ConstantItem &) override;
  void visit(ModuleDecl &) override;
  void visit(StructDecl &) override;
  void visit(EnumDecl &) override;
  void visit(TraitDecl &) override;
  void visit(ImplDecl &) override;
  void visit(RootNode &) override;

private:
  SemanticContext &ctx_;
};

} // namespace rc
