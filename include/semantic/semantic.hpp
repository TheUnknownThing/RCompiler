#pragma once

#include <memory>

#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/firstPass.hpp"
#include "semantic/analyzer/secondPass.hpp"

namespace rc {

class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  void analyze(const std::shared_ptr<RootNode> &root);


private:
  FirstPassBuilder firstPass;
  std::unique_ptr<SecondPassBuilder> secondPass;
};

inline SemanticAnalyzer::SemanticAnalyzer() {
  secondPass = std::make_unique<SecondPassBuilder>(firstPass);
}

inline void SemanticAnalyzer::analyze(const std::shared_ptr<RootNode> &root) {
  firstPass.build(root);
  if (!secondPass)
    secondPass = std::make_unique<SecondPassBuilder>(firstPass);
  secondPass->build(root);
}


} // namespace rc