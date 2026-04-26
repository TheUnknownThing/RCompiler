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
      const std::unordered_map<const BaseNode *, SemType> &expr_cache)
      : expr_cache_(expr_cache) {}

  SemType lookup_type(const BaseNode *node) const;

  TypePtr resolve_type(const SemType &type) const;

private:
  const std::unordered_map<const BaseNode *, SemType>
      &expr_cache_; // from semantic
};



} // namespace rc::ir
