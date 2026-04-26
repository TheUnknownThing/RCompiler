#pragma once

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "ast/nodes/base.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"

#include "instructions/type.hpp"

namespace rc::ir {

class Context {
public:
  explicit Context(
      const std::unordered_map<const BaseNode *, SemType> &exprCache)
      : exprCache_(exprCache) {}

  SemType lookupType(const BaseNode *node) const;

  TypePtr resolveType(const SemType &type) const;

private:
  const std::unordered_map<const BaseNode *, SemType>
      &exprCache_; // from semantic
};



} // namespace rc::ir
