#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/types.hpp"

namespace rc {

class ScopeNode;

enum class ItemKind { Function, Constant, Module, Struct, Enum, Trait };

struct FunctionMetaData {
  std::vector<std::string> param_names;
  std::vector<SemType> param_types;
  SemType return_type;
};
struct StructMetaData {
  std::vector<std::pair<std::string, SemType>> named_fields;
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

  std::variant<std::monostate, FunctionMetaData, StructMetaData, EnumMetaData>
      metadata;

  bool has_function_meta() const {
    return std::holds_alternative<FunctionMetaData>(metadata);
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
  const StructMetaData &as_struct_meta() const {
    return std::get<StructMetaData>(metadata);
  }
  const EnumMetaData &as_enum_meta() const {
    return std::get<EnumMetaData>(metadata);
  }
  FunctionMetaData &as_function_meta() {
    return std::get<FunctionMetaData>(metadata);
  }
  StructMetaData &as_struct_meta() {
    return std::get<StructMetaData>(metadata);
  }
  EnumMetaData &as_enum_meta() { return std::get<EnumMetaData>(metadata); }
};

class ScopeNode {
public:
  explicit ScopeNode(std::string name, ScopeNode *parent = nullptr)
      : name(std::move(name)), parent(parent) {}

  std::string name;
  ScopeNode *parent;

  void add_item(const std::string &name, ItemKind kind, const BaseItem *ast) {
    if (items_.contains(name)) {
      throw SemanticException("duplicate item " + name);
    }
    items_.emplace(name,
                   CollectedItem{name, kind, ast, this, std::monostate()});
  }

  ScopeNode *add_child_scope(std::string name) {
    childNodes.push_back(std::make_unique<ScopeNode>(std::move(name), this));
    return childNodes.back().get();
  }

  std::vector<CollectedItem> items() const {
    std::vector<CollectedItem> out;
    out.reserve(items_.size());
    for (const auto &kv : items_) {
      out.push_back(kv.second);
    }
    return out;
  }

  const CollectedItem *find_item(const std::string &name) const {
    auto it = items_.find(name);
    if (it == items_.end())
      return nullptr;
    return &it->second;
  }

  const std::vector<std::unique_ptr<ScopeNode>> &children() const {
    return childNodes;
  }

  const ScopeNode *find_child_scope(const std::string &name) const {
    for (const auto &c : childNodes) {
      if (c->name == name)
        return c.get();
    }
    return nullptr;
  }
  ScopeNode *find_child_scope(const std::string &name) {
    for (const auto &c : childNodes) {
      if (c->name == name)
        return c.get();
    }
    return nullptr;
  }

private:
  std::map<std::string, CollectedItem> items_;
  std::vector<std::unique_ptr<ScopeNode>> childNodes;
};

inline ScopeNode *enterScope(ScopeNode *&current, const std::string &name) {
  if (!current)
    throw SemanticException("null current scope");
  current = current->add_child_scope(name);
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
    case ItemKind::Module:
      std::cout << "mod";
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
