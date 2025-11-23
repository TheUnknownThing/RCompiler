#pragma once

#include <memory>
#include <unordered_map>

#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/controlAnalyzer.hpp"
#include "semantic/analyzer/dirtyWorkPass.hpp"
#include "semantic/analyzer/firstPass.hpp"
#include "semantic/analyzer/fourthPass.hpp"
#include "semantic/analyzer/secondPass.hpp"
#include "semantic/analyzer/thirdPass.hpp"
#include "semantic/scope.hpp"

namespace rc {

class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  void analyze(const std::shared_ptr<RootNode> &root);

  ScopeNode *root_scope() const { return root_scope_; }
  const std::unordered_map<const BaseNode *, SemType> &expr_cache() const {
    return expr_cache_;
  }

private:
  ScopeNode *root_scope_ = nullptr;
  std::unordered_map<const BaseNode *, SemType> expr_cache_;
};

inline SemanticAnalyzer::SemanticAnalyzer() = default;

inline void SemanticAnalyzer::analyze(const std::shared_ptr<RootNode> &root) {
  // First pass collects item name
  FirstPassBuilder first;
  first.build(root);
  // if (first.root_scope) {
  // std::cout << "[Semantic] Scope tree:" << std::endl;
  // rc::print_scope_tree(*first.root_scope);
  // }

  // Second pass resolves semantic type and evaluates constant expressions
  SecondPassResolver second;
  second.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Third pass promotes impl to struct level
  ThirdPassPromoter third;
  third.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Fourth pass handles let statements and bindings
  FourthPass fourth;
  fourth.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Control analyzer analysis inappropriate continues and breaks
  ControlAnalyzer control_analyzer;
  control_analyzer.analyze(root);

  // Dirty work is just dirty work
  DirtyWorkPass dirty_work;
  dirty_work.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  root_scope_ = first.root_scope;
  expr_cache_ = fourth.getExprCache();
}

} // namespace rc
