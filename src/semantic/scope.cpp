#include "semantic/scope.hpp"

namespace rc {

ScopeNode *enter_scope(ScopeNode *&current, const std::string &name,
                             const BaseNode *owner_node) {
  if (!current)
    throw SemanticException("null current scope");
  current = current->add_child_scope(name, owner_node);
  return current;
}
void exit_scope(ScopeNode *&current) {
  if (!current)
    throw SemanticException("null current scope");
  if (current->parent) {
    current = current->parent;
  } else {
    // Root, do not exit
  }
}
void print_scope_tree(const ScopeNode &scope, int indent) {
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
