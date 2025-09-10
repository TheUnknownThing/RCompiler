#pragma once

#include <memory>

#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/controlAnalyzer.hpp"
#include "semantic/analyzer/constEvaluationPass.hpp"
#include "semantic/analyzer/firstPass.hpp"
#include "semantic/analyzer/secondPass.hpp"
#include "semantic/analyzer/thirdPass.hpp"
#include "semantic/analyzer/fourthPass.hpp"

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
  if (first.root_scope) {
    std::cout << "[Semantic] Scope tree:" << std::endl;
    rc::print_scope_tree(*first.root_scope);
  }

  // Second pass resolves semantic type of items
  SecondPassResolver second;
  second.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Constant evaluation pass
  ConstEvaluationPass const_eval;
  const_eval.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Third pass promotes impl to struct level
  ThirdPassPromoter third;
  third.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Fourth pass handles let statements and bindings
  FourthPass fourth;
  fourth.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Control analyzer analysis inappropriate continues and breaks
  ControlAnalyzer control_analyzer;
  control_analyzer.analyze(root);
  std::cout << "[Semantic] Control flow analysis completed." << std::endl;
}

} // namespace rc