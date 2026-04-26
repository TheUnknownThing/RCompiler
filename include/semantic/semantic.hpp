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
  std::unique_ptr<ScopeNode> prelude_scope_owner_;
  ScopeNode *root_scope_ = nullptr;
  std::unordered_map<const BaseNode *, SemType> expr_cache_;
};



} // namespace rc
