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
  SemanticContext &ctx;
};

inline TypeAnalyzer::TypeAnalyzer(SemanticContext &ctx) : ctx(ctx) {}

inline void TypeAnalyzer::check(const std::shared_ptr<RootNode> &root) {
  visit(*root);
}

inline void TypeAnalyzer::visit(BaseNode &node) {
  if (auto *func = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*func);
  } else if (auto *const_item = dynamic_cast<ConstantItem *>(&node)) {
    visit(*const_item);
  } else if (auto *module = dynamic_cast<ModuleDecl *>(&node)) {
    visit(*module);
  } else if (auto *struct_decl = dynamic_cast<StructDecl *>(&node)) {
    visit(*struct_decl);
  } else if (auto *enum_decl = dynamic_cast<EnumDecl *>(&node)) {
    visit(*enum_decl);
  } else if (auto *trait_decl = dynamic_cast<TraitDecl *>(&node)) {
    visit(*trait_decl);
  } else if (auto *impl_decl = dynamic_cast<ImplDecl *>(&node)) {
    visit(*impl_decl);
  } else if (auto *root_node = dynamic_cast<RootNode *>(&node)) {
    visit(*root_node);
  } else {
    // Do nothing
  }
}

inline void TypeAnalyzer::visit(RootNode &) {}

inline void TypeAnalyzer::visit(FunctionDecl &) {}

inline void TypeAnalyzer::visit(ConstantItem &) {}

inline void TypeAnalyzer::visit(ModuleDecl &) {}

inline void TypeAnalyzer::visit(StructDecl &) {}

inline void TypeAnalyzer::visit(EnumDecl &) {}

inline void TypeAnalyzer::visit(TraitDecl &) {}

inline void TypeAnalyzer::visit(ImplDecl &) {}

} // namespace rc
