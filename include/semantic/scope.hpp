#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/types.hpp"

namespace rc {

struct ConstValue;

class ScopeNode;

enum class ItemKind { Function, Constant, Struct, Enum, Trait };

struct FunctionMetaData {
  std::string name;
  std::vector<std::shared_ptr<BasePattern>> param_names;
  std::vector<SemType> param_types;
  SemType return_type;
  const FunctionDecl *decl = nullptr;
};

struct ConstantMetaData {
  std::string name;
  SemType type;
  const ConstantItem *decl = nullptr;
  std::shared_ptr<ConstValue> evaluated_value;
};

struct StructMetaData {
  std::vector<std::pair<std::string, SemType>> named_fields;

  // impl fields populated in third pass
  std::vector<FunctionMetaData> methods;
  std::vector<ConstantMetaData> constants;
};

struct EnumMetaData {
  std::vector<std::string> variant_names;
};

struct CollectedItem {
  std::string name;
  ItemKind kind;
  const BaseItem *ast_ref;
  const ScopeNode *owner_scope;

  // The below items are populated in the second pass

  std::variant<std::monostate, FunctionMetaData, ConstantMetaData,
               StructMetaData, EnumMetaData>
      metadata;

  bool has_function_meta() const {
    return std::holds_alternative<FunctionMetaData>(metadata);
  }
  bool has_constant_meta() const {
    return std::holds_alternative<ConstantMetaData>(metadata);
  }
  bool has_struct_meta() const {
    return std::holds_alternative<StructMetaData>(metadata);
  }
  bool has_enum_meta() const {
    return std::holds_alternative<EnumMetaData>(metadata);
  }
  const FunctionMetaData &as_function_meta() const {
    return std::get<FunctionMetaData>(metadata);
  }
  const ConstantMetaData &as_constant_meta() const {
    return std::get<ConstantMetaData>(metadata);
  }
  const StructMetaData &as_struct_meta() const {
    return std::get<StructMetaData>(metadata);
  }
  const EnumMetaData &as_enum_meta() const {
    return std::get<EnumMetaData>(metadata);
  }
  FunctionMetaData &as_function_meta() {
    return std::get<FunctionMetaData>(metadata);
  }
  ConstantMetaData &as_constant_meta() {
    return std::get<ConstantMetaData>(metadata);
  }
  StructMetaData &as_struct_meta() {
    return std::get<StructMetaData>(metadata);
  }
  EnumMetaData &as_enum_meta() { return std::get<EnumMetaData>(metadata); }
};

class ScopeNode {
public:
  explicit ScopeNode(std::string name, ScopeNode *parent = nullptr,
                     const BaseNode *owner = nullptr)
      : name(std::move(name)), parent(parent), owner(owner) {}

  std::string name;
  ScopeNode *parent;
  const BaseNode *owner;

  void add_item(const std::string &name, ItemKind kind, const BaseItem *ast) {
    switch (kind) {
    case ItemKind::Function:
    case ItemKind::Constant: {
      if (value_items_.contains(name)) {
        throw SemanticException("duplicate item (value namespace) " + name);
      }
      value_items_.emplace(
          name, CollectedItem{name, kind, ast, this, std::monostate()});
      break;
    }
    case ItemKind::Struct:
    case ItemKind::Enum:
    case ItemKind::Trait: {
      if (type_items_.contains(name)) {
        throw SemanticException("duplicate item (type namespace) " + name);
      }
      type_items_.emplace(
          name, CollectedItem{name, kind, ast, this, std::monostate()});
      break;
    }
    }
  }

  ScopeNode *add_child_scope(std::string name, const BaseNode *owner_node) {
    childNodes.push_back(new ScopeNode(std::move(name), this, owner_node));
    return childNodes.back();
  }

  std::vector<CollectedItem> items() const {
    std::vector<CollectedItem> out;
    out.reserve(value_items_.size() + type_items_.size());
    for (const auto &kv : value_items_)
      out.push_back(kv.second);
    for (const auto &kv : type_items_)
      out.push_back(kv.second);
    return out;
  }

  CollectedItem *find_value_item(const std::string &name) {
    auto it = value_items_.find(name);
    if (it == value_items_.end())
      return nullptr;
    return &it->second;
  }
  const CollectedItem *find_value_item(const std::string &name) const {
    auto it = value_items_.find(name);
    if (it == value_items_.end())
      return nullptr;
    return &it->second;
  }

  CollectedItem *find_type_item(const std::string &name) {
    auto it = type_items_.find(name);
    if (it == type_items_.end())
      return nullptr;
    return &it->second;
  }
  const CollectedItem *find_type_item(const std::string &name) const {
    auto it = type_items_.find(name);
    if (it == type_items_.end())
      return nullptr;
    return &it->second;
  }

  const std::vector<ScopeNode *> &children() const { return childNodes; }

  const ScopeNode *find_child_scope_by_owner(const BaseNode *owner_node) const {
    if (!owner_node)
      return nullptr;
    for (const auto &c : childNodes) {
      if (c->owner == owner_node)
        return c;
    }
    return nullptr;
  }
  ScopeNode *find_child_scope_by_owner(const BaseNode *owner_node) {
    if (!owner_node)
      return nullptr;
    for (const auto &c : childNodes) {
      if (c->owner == owner_node)
        return c;
    }
    return nullptr;
  }

  static SemType resolve_type(const LiteralType &type,
                              ScopeNode *current_scope_node) {
    if (type.is_base()) {
      return SemType::map_primitive(type.as_base());
    }
    if (type.is_tuple()) {
      std::vector<SemType> elem_types;
      elem_types.reserve(type.as_tuple().size());
      for (const auto &elem : type.as_tuple()) {
        elem_types.push_back(resolve_type(elem, current_scope_node));
      }
      return SemType::tuple(std::move(elem_types));
    }
    if (type.is_array()) {
      if (type.as_array().actual_size < 0) {
        throw SemanticException("array size not resolved");
      }
      return SemType::array(
          resolve_type(*type.as_array().element, current_scope_node),
          type.as_array().actual_size);
    }
    if (type.is_slice()) {
      return SemType::slice(
          resolve_type(*type.as_slice().element, current_scope_node));
    }
    if (type.is_path()) {
      const auto &segments = type.as_path().segments;
      if (segments.size() == 1) {
        // Look up the named type in the scope tree
        for (auto *scope = current_scope_node; scope; scope = scope->parent) {
          if (auto *item = scope->find_type_item(segments[0])) {
            return SemType::named(item);
          }
        }
        throw SemanticException("unknown type '" + segments[0] + "'");
      }
      throw SemanticException("qualified paths not supported in types");
    }
    if (type.is_reference()) {
      return SemType::reference(
          resolve_type(*type.as_reference().target, current_scope_node),
          type.as_reference().is_mutable);
    }

    return SemType::primitive(SemPrimitiveKind::UNKNOWN);
  }

private:
  std::map<std::string, CollectedItem> value_items_;
  std::map<std::string, CollectedItem> type_items_;
  std::vector<ScopeNode *> childNodes;
};

inline ScopeNode *enterScope(ScopeNode *&current, const std::string &name,
                             const BaseNode *owner_node) {
  if (!current)
    throw SemanticException("null current scope");
  current = current->add_child_scope(name, owner_node);
  return current;
}

inline void exitScope(ScopeNode *&current) {
  if (!current)
    throw SemanticException("null current scope");
  if (current->parent) {
    current = current->parent;
  } else {
    // Root, do not exit
  }
}

// printer
inline void print_scope_tree(const ScopeNode &scope, int indent = 0) {
  auto indent_str = std::string(indent, ' ');
  if (indent == 0) {
    if (!scope.name.empty()) { // empty name = root
      std::cout << "<root:" << scope.name << ">" << std::endl;
    }
  } else {
    std::cout << indent_str << "scope " << scope.name << std::endl;
  }
  for (const auto &item : scope.items()) {
    std::cout << indent_str << "  item " << item.name << " (";
    switch (item.kind) {
    case ItemKind::Function:
      std::cout << "fn";
      break;
    case ItemKind::Constant:
      std::cout << "const";
      break;
    case ItemKind::Struct:
      std::cout << "struct";
      break;
    case ItemKind::Enum:
      std::cout << "enum";
      break;
    case ItemKind::Trait:
      std::cout << "trait";
      break;
    }
    std::cout << ")" << std::endl;
  }
  for (const auto &child : scope.children()) {
    print_scope_tree(*child, indent + 2);
  }
}

} // namespace rc
