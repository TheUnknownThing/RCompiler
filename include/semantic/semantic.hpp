#pragma once

#include <memory>

#include "ast/nodes/topLevel.hpp"

namespace rc {

class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  void analyze(const std::shared_ptr<RootNode> &root);

private:
};

} // namespace rc