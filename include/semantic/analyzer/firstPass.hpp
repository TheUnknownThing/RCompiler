#pragma once

#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rc {

enum class FirstPassScopeKind { Root, Function, Trait, Block };

enum class FirstPassItemKind {
  Function,
  Constant,
  Struct,
  Enum,
  Trait,
  Parameter
};

struct FirstPassFunction {
  std::vector<std::pair<std::string, LiteralType>> params;
  LiteralType return_type;
};

struct FirstPassStruct {
  bool is_tuple = false;
  std::vector<std::pair<std::string, LiteralType>> fields;
  std::vector<LiteralType> tuple_fields;

  // Second pass metadata
  struct ImplMethod {
    std::string name;
    const FunctionDecl *decl = nullptr;
    FirstPassFunction signature;
  };
  struct ImplConst {
    std::string name;
    const ConstantItem *decl = nullptr;
    LiteralType type;
  };
  struct ImplInfo {
    const ImplDecl *impl_decl = nullptr;
    std::vector<ImplMethod> methods;
    std::vector<ImplConst> consts;
  };
  std::vector<ImplInfo> impls;
  std::unordered_set<std::string> impl_method_names; // for deduplication
  std::unordered_set<std::string> impl_const_names;  // for deduplication
};

struct FirstPassEnum {
  struct Variant {
    std::string name;
  };
  std::vector<Variant> variants;
};

struct FirstPassTrait {};

struct FirstPassItem {
  std::string name;
  FirstPassItemKind kind;
  const BaseNode *ast_node = nullptr; // parent AST node

  std::unique_ptr<FirstPassFunction> function_data;
  std::unique_ptr<FirstPassStruct> struct_data;
  std::unique_ptr<FirstPassEnum> enum_data;
  std::unique_ptr<FirstPassTrait> trait_data;
};

struct FirstPassScopeNode {
  std::size_t id = 0;
  std::size_t depth = 0;
  FirstPassScopeKind kind = FirstPassScopeKind::Root;
  const BaseNode *owner = nullptr;
  std::string label;

  FirstPassScopeNode *parent = nullptr;
  std::vector<std::unique_ptr<FirstPassScopeNode>> children;

  std::unordered_map<std::string, FirstPassItem> items;
};

class FirstPassBuilder : public BaseVisitor {
public:
  FirstPassBuilder();

  void build(const std::shared_ptr<RootNode> &root);

  const FirstPassScopeNode *rootScope() const { return root.get(); }

  const FirstPassItem *lookupLocal(const std::string &name) const;

  FirstPassScopeNode *scopeFor(const BaseNode *owner) {
    auto it = owner_to_scope.find(owner);
    return it == owner_to_scope.end() ? nullptr : it->second;
  }
  const FirstPassScopeNode *scopeFor(const BaseNode *owner) const {
    auto it = owner_to_scope.find(owner);
    return it == owner_to_scope.end() ? nullptr : it->second;
  }

  void visit(BaseNode &node) override;
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(StructDecl &node) override;
  void visit(EnumDecl &node) override;
  void visit(TraitDecl &node) override;
  void visit(RootNode &node) override;
  void visit(BlockExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(MatchExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;

private:
  std::unique_ptr<FirstPassScopeNode> root;
  FirstPassScopeNode *current = nullptr;
  std::size_t next_id_ = 0;
  std::unordered_map<const BaseNode *, FirstPassScopeNode *> owner_to_scope;

  void enterScope(FirstPassScopeKind kind, const BaseNode *owner,
                  std::string label);
  void exitScope();

  FirstPassItem &declareItem(const std::string &name, FirstPassItemKind kind,
                             const BaseNode *ast_node);

  static void ensureUniqueNames(const std::vector<std::string> &names,
                                const std::string &ctx);
};

inline FirstPassBuilder::FirstPassBuilder() {
  root = std::make_unique<FirstPassScopeNode>();
  root->id = next_id_++;
  root->depth = 0;
  root->kind = FirstPassScopeKind::Root;
  root->label = "<root>";
  current = root.get();
}

inline void FirstPassBuilder::build(const std::shared_ptr<RootNode> &root) {
  if (!root)
    return;
  root->accept(*this);
}

inline void FirstPassBuilder::enterScope(FirstPassScopeKind kind,
                                         const BaseNode *owner,
                                         std::string label) {
  auto node = std::make_unique<FirstPassScopeNode>();
  node->id = next_id_++;
  node->depth = current ? current->depth + 1 : 0;
  node->kind = kind;
  node->owner = owner;
  node->label = std::move(label);
  node->parent = current;
  FirstPassScopeNode *raw = node.get();
  if (current) {
    current->children.push_back(std::move(node));
  } else {
    root = std::move(node);
  }
  current = raw;
  if (owner) {
    owner_to_scope[owner] = raw;
  }
}

inline void FirstPassBuilder::exitScope() {
  if (!current)
    return;
  current = current->parent ? current->parent : current;
}

inline FirstPassItem &FirstPassBuilder::declareItem(const std::string &name,
                                                    FirstPassItemKind kind,
                                                    const BaseNode *ast_node) {
  if (!current)
    throw SemanticException("No current scope when declaring " + name);
  auto it = current->items.find(name);
  if (it != current->items.end()) {
    throw SemanticException("Duplicate item in scope: " + name);
  }
  FirstPassItem item{name, kind, ast_node, nullptr, nullptr, nullptr, nullptr};
  auto res = current->items.emplace(name, std::move(item));
  return res.first->second;
}

inline const FirstPassItem *
FirstPassBuilder::lookupLocal(const std::string &name) const {
  if (!current)
    return nullptr;
  auto it = current->items.find(name);
  if (it == current->items.end())
    return nullptr;
  return &it->second;
}

inline void
FirstPassBuilder::ensureUniqueNames(const std::vector<std::string> &names,
                                    const std::string &ctx) {
  std::unordered_set<std::string> seen;
  for (const auto &n : names) {
    if (!seen.insert(n).second) {
      throw SemanticException("Duplicate of " + ctx + " " + n);
    }
  }
}

inline void FirstPassBuilder::visit(BaseNode &node) {
  if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<ConstantItem *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<StructDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<EnumDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<TraitDecl *>(&node)) {
    visit(*decl);
  } else if (auto *root = dynamic_cast<RootNode *>(&node)) {
    visit(*root);
  } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<MatchExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
    visit(*expr);
  }
  // Expressions, statements, patterns ignored in first pass.
}

inline void FirstPassBuilder::visit(RootNode &node) {
  for (auto &child : node.children) {
    if (child)
      child->accept(*this);
  }
}

inline void FirstPassBuilder::visit(FunctionDecl &node) {
  auto &item = declareItem(node.name, FirstPassItemKind::Function, &node);
  item.function_data = std::make_unique<FirstPassFunction>();
  auto &meta = *item.function_data;
  meta.return_type = node.return_type;
  if (node.params) {
    std::vector<std::string> names;
    for (const auto &p : *node.params) {
      names.push_back(p.first);
    }
    ensureUniqueNames(names, "parameter");
    meta.params = *node.params;
  }

  enterScope(FirstPassScopeKind::Function, &node, node.name);
  if (node.params) {
    for (const auto &p : *node.params) {
      FirstPassItem paramItem{p.first, FirstPassItemKind::Parameter,
                              &node,   nullptr,
                              nullptr, nullptr,
                              nullptr};
      if (current->items.count(p.first)) {
        throw SemanticException("Parameter already declared: " + p.first);
      }
      current->items.emplace(p.first, std::move(paramItem));
    }
  }
  if (node.body && *node.body) {
    (*node.body)->accept(*this);
  }
  exitScope();
}

inline void FirstPassBuilder::visit(ConstantItem &node) {
  declareItem(node.name, FirstPassItemKind::Constant, &node);
}

inline void FirstPassBuilder::visit(StructDecl &node) {
  auto &item = declareItem(node.name, FirstPassItemKind::Struct, &node);
  item.struct_data = std::make_unique<FirstPassStruct>();
  auto &meta = *item.struct_data;
  meta.is_tuple = (node.struct_type == StructDecl::StructType::Tuple);
  if (meta.is_tuple) {
    meta.tuple_fields = node.tuple_fields;
  } else {
    std::vector<std::string> names;
    for (const auto &f : node.fields) {
      names.push_back(f.first);
    }
    ensureUniqueNames(names, "field");
    meta.fields = node.fields;
  }
}

inline void FirstPassBuilder::visit(EnumDecl &node) {
  auto &item = declareItem(node.name, FirstPassItemKind::Enum, &node);
  item.enum_data = std::make_unique<FirstPassEnum>();
  auto &meta = *item.enum_data;
  std::vector<std::string> names;
  for (const auto &v : node.variants) {
    names.push_back(v.name);
    meta.variants.push_back(FirstPassEnum::Variant{v.name});
  }
  ensureUniqueNames(names, "variant");
}

inline void FirstPassBuilder::visit(TraitDecl &node) {
  auto &item = declareItem(node.name, FirstPassItemKind::Trait, &node);
  item.trait_data = std::make_unique<FirstPassTrait>();
  enterScope(FirstPassScopeKind::Trait, &node, node.name);
  for (auto &assoc : node.associated_items) {
    if (assoc)
      assoc->accept(*this);
  }
  exitScope();
}

inline void FirstPassBuilder::visit(BlockExpression &node) {
  enterScope(FirstPassScopeKind::Block, &node, "block");
  for (auto &stmt : node.statements) {
    if (stmt)
      stmt->accept(*this);
  }
  exitScope();
}

inline void FirstPassBuilder::visit(IfExpression &node) {
  if (node.then_block) {
    node.then_block->accept(*this);
  }
  if (node.else_block && *node.else_block) {
    (*node.else_block)->accept(*this);
  }
}

inline void FirstPassBuilder::visit(MatchExpression &node) {
  for (auto &arm : node.arms) {
    if (arm.body)
      arm.body->accept(*this);
  }
}

inline void FirstPassBuilder::visit(LoopExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

inline void FirstPassBuilder::visit(WhileExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

} // namespace rc
