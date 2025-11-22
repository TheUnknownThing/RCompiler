#pragma once

#include <memory>
#include <stdexcept>
#include <string>
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
      const rc::ScopeNode &rootScope,
      const std::unordered_map<const BaseNode *, SemType> &exprCache)
      : rootScope_(rootScope), exprCache_(exprCache) {}

  void enterScope(const rc::ScopeNode &scope) { currentScope_ = &scope; }

  const rc::ScopeNode *exitScope() {
    if (currentScope_->parent) {
      currentScope_ = currentScope_->parent;
      return currentScope_;
    }
    return nullptr; // at root
  }

  SemType lookupType(const BaseNode *node) const;

  TypePtr resolveType(const SemType &type) const;

private:
  const rc::ScopeNode &rootScope_;
  const rc::ScopeNode *currentScope_ = &rootScope_;
  const std::unordered_map<const BaseNode *, SemType> &exprCache_; // from semantic
};

inline SemType Context::lookupType(const BaseNode *node) const {
  auto it = exprCache_.find(node);
  if (it == exprCache_.end()) {
    throw std::runtime_error("Context::lookupType: type not found for node");
  }
  return it->second;
}

inline TypePtr Context::resolveType(const SemType &type) const {
  if (type.is_primitive()) {
    switch (type.as_primitive().kind) {
    case SemPrimitiveKind::BOOL:
      return IntegerType::i1();
    case SemPrimitiveKind::I32:
      return IntegerType::i32(true);
    case SemPrimitiveKind::U32:
      return IntegerType::i32(false);
    case SemPrimitiveKind::ISIZE:
      return IntegerType::isize();
    case SemPrimitiveKind::USIZE:
      return IntegerType::usize();
    case SemPrimitiveKind::UNIT:
      return std::make_shared<UnitZstType>();
    case SemPrimitiveKind::NEVER:
      return std::make_shared<VoidType>();
    case SemPrimitiveKind::ANY_INT:
      // ANY_INT should have been resolved by semantic analysis
      // throw error here
      throw std::runtime_error(
          "Context::resolveType: ANY_INT should have been resolved");
      // return IntegerType::i32(true);
    default:
      throw std::runtime_error(
          "Context::resolveType: unsupported primitive kind");
    }
  }
  if (type.is_reference()) {
    return std::make_shared<PointerType>(
        resolveType(*type.as_reference().target));
  }
  if (type.is_array()) {
    return std::make_shared<ArrayType>(
        resolveType(*type.as_array().element), type.as_array().size);
  }
  if (type.is_tuple()) {
    std::vector<TypePtr> elems;
    elems.reserve(type.as_tuple().elements.size());
    for (const auto &elem : type.as_tuple().elements) {
      elems.push_back(resolveType(elem));
    }
    return std::make_shared<StructType>(std::move(elems));
  }
  if (type.is_named()) {
    const auto *item = type.as_named().item;
    if (!item)
      throw std::runtime_error(
          "Context::resolveType: named item missing metadata");
    if (item->kind == ItemKind::Struct && item->has_struct_meta()) {
      std::vector<TypePtr> fields;
      for (const auto &field : item->as_struct_meta().named_fields) {
        fields.push_back(resolveType(field.second));
      }
      return std::make_shared<StructType>(std::move(fields), item->name);
    }
    throw std::runtime_error(
        "Context::resolveType: unsupported named item kind");
  }
  throw std::runtime_error("Context::resolveType: unsupported type kind");
}


} // namespace rc::ir
