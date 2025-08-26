#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"

namespace rc {

class SecondPassResolver : public BaseVisitor {
public:
  SecondPassResolver() = default;

  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_) {
    root_scope = root_scope_;
    if (!root_scope)
      throw SemanticException("second pass requires root scope");
    scope_stack.clear();
    scope_stack.push_back(root_scope);
    if (root) {
      for (const auto &child : root->children) {
        if (child)
          child->accept(*this);
      }
    }
  }

  void visit(BaseNode &node) override {
    if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
      visit(*decl);
    } else if (auto *decl = dynamic_cast<StructDecl *>(&node)) {
      visit(*decl);
    } else if (auto *cst = dynamic_cast<ConstantItem *>(&node)) {
      visit(*cst);
    } else if (auto *decl = dynamic_cast<EnumDecl *>(&node)) {
      visit(*decl);
    } else if (auto *decl = dynamic_cast<TraitDecl *>(&node)) {
      visit(*decl);
    } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
      visit(*expr);
    }
  }

  void visit(FunctionDecl &node) override {
    FunctionMetaData sig;
    sig.name = node.name;
    sig.decl = &node;
    std::set<std::string> seen;
    if (node.params) {
      for (const auto &p : *node.params) {
        const auto &name = p.first;
        if (seen.contains(name)) {
          throw SemanticException("duplicate parameter '" + name +
                                  "' in function '" + node.name + "'");
        }
        seen.insert(name);
        sig.param_names.push_back(name);
        sig.param_types.push_back(resolve_type(p.second));
      }
    }
    sig.return_type = resolve_type(node.return_type);
    if (auto *ci = lookup_current_item(node.name, ItemKind::Function)) {
      ci->metadata = std::move(sig);
    }

    if (node.body && node.body.value()) {
      node.body.value()->accept(*this);
    }
  }

  void visit(ConstantItem &node) override {
    ConstantMetaData meta;
    meta.name = node.name;
    meta.decl = &node;
    meta.type = resolve_type(node.type);
  }

  void visit(StructDecl &node) override {
    StructMetaData info;
    if (node.struct_type == StructDecl::StructType::Struct) {
      std::set<std::string> seen;
      for (const auto &field : node.fields) {
        const auto &name = field.first;
        if (seen.contains(name)) {
          throw SemanticException("duplicate field " + name);
        }
        seen.insert(name);
        info.named_fields.emplace_back(name, resolve_type(field.second));
      }
    } else {
      // No Tuple Struct Now
    }
    if (auto *ci = lookup_current_item(node.name, ItemKind::Struct)) {
      ci->metadata = std::move(info);
    }
  }

  void visit(EnumDecl &node) override {
    std::set<std::string> seen;
    EnumMetaData info;
    for (const auto &variant : node.variants) {
      if (seen.contains(variant.name)) {
        throw SemanticException("duplicate enum variant " + variant.name);
      }
      seen.insert(variant.name);
      info.variant_names.push_back(variant.name);
    }
    if (auto *ci = lookup_current_item(node.name, ItemKind::Enum)) {
      ci->metadata = std::move(info);
    }
  }

  void visit(TraitDecl &node) override {
    auto *parent_scope = current_scope();
    auto *trait_scope = parent_scope->find_child_scope(node.name);

    push_scope(trait_scope);
    for (const auto &assoc : node.associated_items) {
      if (assoc)
        assoc->accept(*this);
    }
    pop_scope();
  }

  void visit(BlockExpression &node) override {
    for (const auto &stmt : node.statements) {
      if (stmt)
        stmt->accept(*this);
    }
    if (node.final_expr)
      node.final_expr.value()->accept(*this);
  }

  void visit(IfExpression &node) override {
    if (node.then_block)
      node.then_block->accept(*this);
    if (node.else_block)
      node.else_block.value()->accept(*this);
  }

  void visit(LoopExpression &node) override {
    if (node.body)
      node.body->accept(*this);
  }

  void visit(WhileExpression &node) override {
    if (node.body)
      node.body->accept(*this);
  }

  void visit(RootNode &) override {}

private:
  ScopeNode *root_scope = nullptr;
  std::vector<ScopeNode *> scope_stack;

  ScopeNode *current_scope() const { return scope_stack.back(); }

  void push_scope(ScopeNode *s) { scope_stack.push_back(s); }
  void pop_scope() {
    if (scope_stack.size() > 1)
      scope_stack.pop_back();
  }

  SemType resolve_type(const LiteralType &t) {
    if (t.is_base()) {
      return SemType::primitive(map_primitive(t.as_base()));
    }
    if (t.is_tuple()) {
      std::vector<SemType> elems;
      elems.reserve(t.as_tuple().size());
      for (const auto &el : t.as_tuple()) {
        elems.push_back(resolve_type(el));
      }
      return SemType::tuple(std::move(elems));
    }
    if (t.is_array()) {
      return SemType::array(resolve_type(*t.as_array().element),
                            t.as_array().size);
    }
    if (t.is_slice()) {
      return SemType::slice(resolve_type(*t.as_slice().element));
    }
    if (t.is_path()) {
      const auto &segs = t.as_path().segments;
      if (segs.empty())
        throw SemanticException("empty path");
      if (segs.size() == 1) {
        const auto *ci = resolve_named_item(segs[0]);
        if (ci)
          return SemType::named(ci);
        throw SemanticException("unknown named item " + segs[0]);
      }
      // qualified path, no such thing
      throw SemanticException("qualified path not supported");
    }
    throw SemanticException("unknown type");
  }

  const CollectedItem *resolve_named_item(const std::string &name) const {
    for (auto *scope = current_scope(); scope; scope = scope->parent) {
      if (const auto *ci = scope->find_item(name)) {
        if (ci->kind == ItemKind::Struct || ci->kind == ItemKind::Enum) {
          return ci;
        }
      }
    }
    return nullptr;
  }

  SemPrimitiveKind map_primitive(PrimitiveLiteralType plt) {
    switch (plt) {
    case PrimitiveLiteralType::I32:
      return SemPrimitiveKind::I32;
    case PrimitiveLiteralType::U32:
      return SemPrimitiveKind::U32;
    case PrimitiveLiteralType::ISIZE:
      return SemPrimitiveKind::ISIZE;
    case PrimitiveLiteralType::USIZE:
      return SemPrimitiveKind::USIZE;
    case PrimitiveLiteralType::STRING:
      return SemPrimitiveKind::STRING;
    case PrimitiveLiteralType::RAW_STRING:
      return SemPrimitiveKind::RAW_STRING;
    case PrimitiveLiteralType::C_STRING:
      return SemPrimitiveKind::C_STRING;
    case PrimitiveLiteralType::RAW_C_STRING:
      return SemPrimitiveKind::RAW_C_STRING;
    case PrimitiveLiteralType::CHAR:
      return SemPrimitiveKind::CHAR;
    case PrimitiveLiteralType::BOOL:
      return SemPrimitiveKind::BOOL;
    case PrimitiveLiteralType::NEVER:
      return SemPrimitiveKind::NEVER;
    case PrimitiveLiteralType::UNIT:
      return SemPrimitiveKind::UNIT;
    }
    return SemPrimitiveKind::UNKNOWN;
  }

  CollectedItem *lookup_current_item(const std::string &name, ItemKind kind) {
    auto *scope = current_scope();
    auto *found = const_cast<CollectedItem *>(scope->find_item(name));
    if (found && found->kind == kind)
      return found;
    throw SemanticException("item not found");
  }
};

} // namespace rc
