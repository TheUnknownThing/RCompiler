#pragma once

#include "ast/nodes/topLevel.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"

#include <unordered_set>

namespace rc {

class ThirdPassPromoter : public BaseVisitor {
public:
  ThirdPassPromoter() = default;

  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_) {
    root_scope = root_scope_;
    if (!root_scope)
      throw SemanticException("third pass requires existing root scope");
    current_scope = root_scope;
    if (root) {
      for (const auto &child : root->children) {
        if (child)
          child->accept(*this);
      }
    }
  }

  void visit(BaseNode &node) override {
    if (auto *impl = dynamic_cast<ImplDecl *>(&node)) {
      visit(*impl);
    } else if (auto *fn = dynamic_cast<FunctionDecl *>(&node)) {
      visit(*fn);
    } else if (auto *tr = dynamic_cast<TraitDecl *>(&node)) {
      visit(*tr);
    } else if (auto *blk = dynamic_cast<BlockExpression *>(&node)) {
      visit(*blk);
    } else if (auto *ife = dynamic_cast<IfExpression *>(&node)) {
      visit(*ife);
    } else if (auto *lop = dynamic_cast<LoopExpression *>(&node)) {
      visit(*lop);
    } else if (auto *whl = dynamic_cast<WhileExpression *>(&node)) {
      visit(*whl);
    }
  }

  void visit(FunctionDecl &node) override {
    if (node.body && node.body.value()) {
      auto expr = node.body.value();
      expr->accept(*this);
    }
  }

  void visit(ImplDecl &node) override {
    if (node.impl_type != ImplDecl::ImplType::Inherent) {
      // Trait impls not handled
      return;
    }

    std::string target_name;
    if (node.target_type.is_path()) {
      const auto &segs = node.target_type.as_path().segments;
      if (segs.size() == 1) {
        target_name = segs[0];
      } else {
        // not support qualified path
        return;
      }
    } else {
      throw SemanticException("unsupported target type");
    }

    CollectedItem *struct_item = resolve_struct(target_name);
    if (!struct_item) {
      throw SemanticException("impl target " + target_name + " not found");
    }
    if (!struct_item->has_struct_meta()) {
      throw SemanticException("impl target " + target_name + " is not struct");
    }
    auto &meta = struct_item->as_struct_meta();

    std::unordered_set<std::string> existing;

    for (const auto &m : meta.methods)
      existing.insert(m.name);
    for (const auto &c : meta.constants)
      existing.insert(c.name);

    for (const auto &assoc : node.associated_items) {
      if (!assoc)
        continue;
      if (auto *fn = dynamic_cast<FunctionDecl *>(assoc.get())) {
        if (existing.contains(fn->name)) {
          throw SemanticException("duplicate name " + fn->name + " in impl");
        }
        existing.insert(fn->name);
        FunctionMetaData fmd;
        fmd.name = fn->name;
        if (fn->params) {
          for (const auto &p : *fn->params) {
            fmd.param_names.push_back(p.first);
            fmd.param_types.push_back(
                resolve_type(p.second, struct_item->owner_scope));
          }
        }
        fmd.return_type =
            resolve_type(fn->return_type, struct_item->owner_scope);
        fmd.decl = fn;
        meta.methods.push_back(std::move(fmd));
      } else if (auto *cst = dynamic_cast<ConstantItem *>(assoc.get())) {
        if (existing.contains(cst->name)) {
          throw SemanticException("duplicate name " + cst->name + " in impl");
        }
        existing.insert(cst->name);
        ConstantMetaData cmd;
        cmd.name = cst->name;
        cmd.type = resolve_type(cst->type, struct_item->owner_scope);
        cmd.decl = cst;
        meta.constants.push_back(std::move(cmd));
      }
    }
  }

  void visit(TraitDecl &) override {
    // TODO: I do not want to care about it now.
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
  ScopeNode *current_scope = nullptr;

  CollectedItem *resolve_struct(const std::string &name) const {
    for (auto *scope = current_scope; scope; scope = scope->parent) {
      auto *ci = const_cast<CollectedItem *>(scope->find_item(name));
      if (ci && ci->kind == ItemKind::Struct)
        return ci;
    }
    return nullptr;
  }

  SemType resolve_type(const LiteralType &t, const ScopeNode *base_scope) {
    if (t.is_base()) {
      return SemType::primitive(map_primitive(t.as_base()));
    }
    if (t.is_tuple()) {
      std::vector<SemType> elems;
      elems.reserve(t.as_tuple().size());
      for (const auto &el : t.as_tuple()) {
        elems.push_back(resolve_type(el, base_scope));
      }
      return SemType::tuple(std::move(elems));
    }
    if (t.is_array()) {
      return SemType::array(resolve_type(*t.as_array().element, base_scope),
                            t.as_array().size);
    }
    if (t.is_slice()) {
      return SemType::slice(resolve_type(*t.as_slice().element, base_scope));
    }
    if (t.is_path()) {
      const auto &segs = t.as_path().segments;
      if (segs.empty())
        throw SemanticException("empty path");
      if (segs.size() == 1) {
        const auto *ci = resolve_named_item(segs[0], base_scope);
        if (ci)
          return SemType::named(ci);
        throw SemanticException("unknown named item " + segs[0]);
      }
      // qualified path, no such thing
      throw SemanticException("qualified path not supported");
    }
    throw SemanticException("unknown type");
  }

  const CollectedItem *resolve_named_item(const std::string &name,
                                          const ScopeNode *start) const {
    for (auto *scope = const_cast<ScopeNode *>(start); scope;
         scope = scope->parent) {
      if (const auto *ci = scope->find_item(name))
        return ci;
    }
    return nullptr;
  }

  SemPrimitiveKind map_primitive(PrimitiveLiteralType plt) const {
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
};

} // namespace rc
