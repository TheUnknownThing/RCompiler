#pragma once

#include <memory>
#include <unordered_map>

#include "ast/nodes/top_level.hpp"
#include "semantic/analyzer/control_analyzer.hpp"
#include "semantic/analyzer/dirty_work_pass.hpp"
#include "semantic/analyzer/first_pass.hpp"
#include "semantic/analyzer/fourth_pass.hpp"
#include "semantic/analyzer/second_pass.hpp"
#include "semantic/analyzer/third_pass.hpp"
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
