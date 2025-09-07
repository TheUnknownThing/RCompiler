#pragma once

#include <memory>

#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/controlAnalyzer.hpp"
#include "semantic/analyzer/constEvaluationPass.hpp"
#include "semantic/analyzer/firstPass.hpp"
#include "semantic/analyzer/secondPass.hpp"
#include "semantic/analyzer/thirdPass.hpp"

namespace rc {

class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  void analyze(const std::shared_ptr<RootNode> &root);
};

inline SemanticAnalyzer::SemanticAnalyzer() = default;

inline void SemanticAnalyzer::analyze(const std::shared_ptr<RootNode> &root) {

  // First pass collects item name
  FirstPassBuilder first;
  first.build(root);
  std::cout << "\n[Semantic] First pass completed." << std::endl;
  if (first.root_scope) {
    std::cout << "[Semantic] Scope tree:" << std::endl;
    rc::print_scope_tree(*first.root_scope);
  }

  // Second pass resolves semantic type of items
  SecondPassResolver second;
  second.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);
  std::cout << "[Semantic] Second pass completed." << std::endl;

  // Constant evaluation pass
  ConstEvaluationPass const_eval;
  const_eval.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);
  std::cout << "[Semantic] Constant evaluation pass completed." << std::endl;

  // Third pass promotes impl to struct level
  ThirdPassPromoter third;
  third.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);
  std::cout << "[Semantic] Third pass completed." << std::endl;

  // Control analyzer analysis inappropriate continues and breaks
  ControlAnalyzer control_analyzer;
  control_analyzer.analyze(root);
  std::cout << "[Semantic] Control flow analysis completed." << std::endl;
}

} // namespace rc