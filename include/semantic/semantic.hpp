#pragma once

#include <memory>

#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/controlAnalyzer.hpp"
#include "semantic/analyzer/firstPass.hpp"
#include "semantic/analyzer/secondPass.hpp"

namespace rc {

class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  void analyze(const std::shared_ptr<RootNode> &root);
};

SemanticAnalyzer::SemanticAnalyzer() = default;

inline void SemanticAnalyzer::analyze(const std::shared_ptr<RootNode> &root) {
  // Run first pass
  FirstPassBuilder first;
  first.build(root);
  std::cout << "\n[Semantic] First pass (scope & item collection) completed."
            << std::endl;
  if (first.root_scope) {
    std::cout << "[Semantic] Scope tree:" << std::endl;
    rc::print_scope_tree(*first.root_scope);
  }

  // Run second pass
  SecondPassResolver second;
  second.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);
  std::cout << "[Semantic] Second pass (signatures & member validation) completed." << std::endl;

  ControlAnalyzer control_analyzer;
  control_analyzer.analyze(root);
  std::cout << "[Semantic] Control flow analysis completed." << std::endl;
}

} // namespace rc