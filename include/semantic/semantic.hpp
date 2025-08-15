#pragma once

#include <memory>

#include "ast/nodes/topLevel.hpp"
#include "semantic/context/semanticContext.hpp"

namespace rc {

class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  void analyze(const std::shared_ptr<RootNode> &root);

private:
  SemanticContext ctx_;
};

} // namespace rc