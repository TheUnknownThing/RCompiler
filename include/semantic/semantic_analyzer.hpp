#pragma once

#include "ast/nodes/pattern.hpp"
#include "ast/utils/parsec.hpp"

#include "lexer/lexer.hpp"

namespace rc {

class SemanticAnalyzer {
public:
  SemanticAnalyzer() = default;

  void analyze(const std::shared_ptr<RootNode> &root) {
    // TODO
  }
};

} // namespace rc