#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/pattern.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/constEvaluator.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "utils/logger.hpp"

namespace rc {

class SecondPassResolver : public BaseVisitor {
public:
  SecondPassResolver();

  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_);

  void visit(BaseNode &node) override;
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(StructDecl &node) override;
  void visit(EnumDecl &node) override;
  void visit(TraitDecl &node) override;
  void visit(ImplDecl &node) override;
  void visit(BlockExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(LetStatement &node) override;
  void visit(ArrayExpression &node) override;
  void visit(BorrowExpression &node) override;
  void visit(DerefExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(PrefixExpression &node) override;
  void visit(ExpressionStatement &node) override;
  void visit(StructExpression &node) override;
  void visit(RootNode &) override;

private:
  ScopeNode *root_scope;
  std::vector<ScopeNode *> scope_stack;
  ConstEvaluator evaluator;

  ScopeNode *current_scope() const;
  void enterScope(ScopeNode *s);
  void exitScope();
  SemType resolve_type(LiteralType &t);
  const CollectedItem *resolve_named_item(const std::string &name) const;
  CollectedItem *lookup_current_value_item(const std::string &name,
                                           ItemKind kind);
  CollectedItem *lookup_current_type_item(const std::string &name,
                                          ItemKind kind);
};

// Implementation

inline SecondPassResolver::SecondPassResolver()
    : root_scope(nullptr), evaluator(this) {}

inline void SecondPassResolver::run(const std::shared_ptr<RootNode> &root,
                                    ScopeNode *root_scope_) {
  LOG_INFO("[SecondPass] Starting unified semantic analysis");
  root_scope = root_scope_;
  if (!root_scope) {
    throw SemanticException(
        "Unified semantic pass requires a valid root scope");
  }

  scope_stack.clear();
  scope_stack.push_back(root_scope);

  if (root) {
    size_t idx = 0;
    for (const auto &child : root->children) {
      if (child && dynamic_cast<ConstantItem *>(child.get())) {
        LOG_DEBUG("[SecondPass] Visiting constant item #" +
                  std::to_string(idx));
        child->accept(*this);
      }
      ++idx;
    }

    for (const auto &child : root->children) {
      if (child && !dynamic_cast<ConstantItem *>(child.get())) {
        LOG_DEBUG("[SecondPass] Visiting top-level child #" +
                  std::to_string(idx));
        child->accept(*this);
      }
      ++idx;
    }
  }

  LOG_INFO("[SecondPass] Completed");
}

inline void SecondPassResolver::visit(BaseNode &node) {
  if (auto *cst = dynamic_cast<ConstantItem *>(&node)) {
    visit(*cst);
  } else if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<StructDecl *>(&node)) {
    visit(*decl);
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
  } else if (auto *expr = dynamic_cast<BorrowExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<DerefExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BinaryExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<PrefixExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*expr);
  }
  // we only care about arr type resolution below
  else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *decl = dynamic_cast<ImplDecl *>(&node)) {
    visit(*decl);
  } else if (auto *expr = dynamic_cast<ArrayExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<StructExpression *>(&node)) {
    visit(*expr);
  }
}

inline void SecondPassResolver::visit(FunctionDecl &node) {
  LOG_DEBUG("[SecondPass] Resolve function '" + node.name + "'");

  FunctionMetaData sig;
  sig.name = node.name;
  sig.decl = &node;

  if (node.params) {
    // TODO: deduplicate parameter names
    std::set<std::shared_ptr<BasePattern>> seen_param_names;
    for (auto &p : *node.params) {
      const auto &name = p.first;
      if (!seen_param_names.insert(name).second) {
        throw SemanticException("duplicate parameter in function '" +
                                node.name + "'");
      }
      sig.param_names.push_back(name);
      sig.param_types.push_back(resolve_type(p.second));
    }
  }

  sig.return_type = resolve_type(node.return_type);

  if (sig.name == "main") {
    if (!(sig.return_type == SemType::primitive(SemPrimitiveKind::UNIT))) {
      throw SemanticException(
          "main function must have return type '()', got '" +
          to_string(sig.return_type) + "'");
    }
  }

  if (auto *ci = lookup_current_value_item(node.name, ItemKind::Function)) {
    ci->metadata = std::move(sig);
  }

  if (node.body && node.body.value()) {
    node.body.value()->accept(*this);
  }
}

inline void SecondPassResolver::visit(ConstantItem &node) {
  LOG_DEBUG("[SecondPass] Resolve and evaluate constant '" + node.name + "'");

  auto *ci = lookup_current_value_item(node.name, ItemKind::Constant);
  ConstantMetaData meta;
  meta.name = node.name;
  meta.decl = &node;
  meta.type = resolve_type(node.type);
  ci->metadata = meta;

  if (node.value) {
    try {
      auto evaluated = evaluator.evaluate(node.value->get(), current_scope());
      if (evaluated) {
        ci->as_constant_meta().evaluated_value =
            std::make_shared<ConstValue>(std::move(*evaluated));
        LOG_DEBUG("[SecondPass] Successfully evaluated constant '" + node.name +
                  "'");
      } else {
        throw SemanticException("Could not evaluate constant '" + node.name +
                                "' - not a constant expression");
      }
    } catch (const std::exception &e) {
      LOG_ERROR(std::string("[SecondPass] Error evaluating constant '") +
                node.name + "': " + e.what());
      throw;
    }
  }
}

inline void SecondPassResolver::visit(StructDecl &node) {
  LOG_DEBUG("[SecondPass] Resolve struct '" + node.name + "'");
  StructMetaData info;

  if (node.struct_type == StructDecl::StructType::Struct) {
    std::set<std::string> seen;
    for (auto &field : node.fields) {
      const auto &fname = field.first;
      if (!seen.insert(fname).second) {
        LOG_ERROR("[SecondPass] Duplicate struct field '" + fname + "'");
        throw SemanticException("duplicate field " + fname);
      }
      info.named_fields.emplace_back(fname, resolve_type(field.second));
    }
  } else {
    // Tuple structs removed
  }

  if (auto *ci = lookup_current_type_item(node.name, ItemKind::Struct)) {
    ci->metadata = std::move(info);
  }
}

inline void SecondPassResolver::visit(EnumDecl &node) {
  LOG_DEBUG("[SecondPass] Resolve enum '" + node.name + "'");
  std::set<std::string> seen;
  EnumMetaData info;
  for (const auto &variant : node.variants) {
    if (!seen.insert(variant.name).second) {
      LOG_ERROR("[SecondPass] Duplicate enum variant '" + variant.name + "'");
      throw SemanticException("duplicate enum variant " + variant.name);
    }
    info.variant_names.push_back(variant.name);
  }
  if (auto *ci = lookup_current_type_item(node.name, ItemKind::Enum)) {
    ci->metadata = std::move(info);
  }
}

inline void SecondPassResolver::visit(TraitDecl &node) {
  auto *parent_scope = current_scope();
  auto *trait_scope =
      parent_scope ? parent_scope->find_child_scope_by_owner(&node) : nullptr;

  LOG_DEBUG("[SecondPass] Enter trait '" + node.name + "'");
  enterScope(trait_scope);
  for (const auto &assoc : node.associated_items) {
    if (assoc && dynamic_cast<ConstantItem *>(assoc.get()))
      assoc->accept(*this);
  }

  for (const auto &assoc : node.associated_items) {
    if (assoc && !dynamic_cast<ConstantItem *>(assoc.get()))
      assoc->accept(*this);
  }
  exitScope();
  LOG_DEBUG("[SecondPass] Exit trait '" + node.name + "'");
}

inline void SecondPassResolver::visit(ImplDecl &node) {
  for (auto &assoc : node.associated_items) {
    if (assoc) {
      // assoc->accept(*this);
      if (auto *fn = dynamic_cast<FunctionDecl *>(assoc.get())) {
        if (fn->params) {
          for (auto &p : *fn->params) {
            SemType pt = resolve_type(p.second);
            if (pt.is_array() && pt.as_array().size < 0) {
              throw SemanticException("array size not resolved");
            }
          }
        }
        SemType rt = resolve_type(fn->return_type);
        if (rt.is_array() && rt.as_array().size < 0) {
          throw SemanticException("array size not resolved");
        }
      } else if (auto *cst = dynamic_cast<ConstantItem *>(assoc.get())) {
        SemType ct = resolve_type(cst->type);
        if (ct.is_array() && ct.as_array().size < 0) {
          throw SemanticException("array size not resolved");
        }
      }
    }
  }
}

inline void SecondPassResolver::visit(BlockExpression &node) {
  auto *block_scope = current_scope()
                          ? current_scope()->find_child_scope_by_owner(&node)
                          : nullptr;
  enterScope(block_scope);

  for (const auto &stmt : node.statements) {
    if (stmt && dynamic_cast<ConstantItem *>(stmt.get()))
      stmt->accept(*this);
  }

  for (const auto &stmt : node.statements) {
    if (stmt && !dynamic_cast<ConstantItem *>(stmt.get()))
      stmt->accept(*this);
  }

  if (node.final_expr) {
    node.final_expr.value()->accept(*this);
  }

  exitScope();
}

inline void SecondPassResolver::visit(IfExpression &node) {
  if (node.then_block)
    node.then_block->accept(*this);
  if (node.else_block)
    node.else_block.value()->accept(*this);
}

inline void SecondPassResolver::visit(LoopExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

inline void SecondPassResolver::visit(WhileExpression &node) {
  if (node.body)
    node.body->accept(*this);
}

inline void SecondPassResolver::visit(LetStatement &node) {
  SemType annotated = resolve_type(node.type);
  if (node.type.is_array()) {
    LOG_DEBUG("[SecondPass] Let statement with array type of size " +
              std::to_string(annotated.as_array().size));
  }
  if (node.expr) {
    node.expr->accept(*this);
  }
}

inline void SecondPassResolver::visit(ArrayExpression &node) {
  if (node.repeat) {
    auto cv = evaluator.evaluate(node.repeat->second.get(), current_scope());
    if (!cv) {
      throw SemanticException("array size is not a constant expression");
    }

    std::uint64_t size_val = 0;
    if (cv->is_usize()) {
      size_val = cv->as_usize();
    } else if (cv->is_any_int()) {
      auto v = cv->as_any_int();
      if (v < 0)
        throw SemanticException("array size cannot be negative");
      size_val = static_cast<std::uint64_t>(v);
    } else {
      throw SemanticException("array size must be usize");
    }

    LOG_DEBUG("[SecondPass] Resolved ArrayExpr size: " +
              std::to_string(size_val));

    node.actual_size = static_cast<int64_t>(size_val);

    node.repeat->first->accept(*this);
  } else {
    for (const auto &element : node.elements) {
      element->accept(*this);
    }
  }
  LOG_DEBUG("[SecondPass] ArrayExpr with size " +
            std::to_string(node.actual_size));
}

inline void SecondPassResolver::visit(BorrowExpression &node) {
  if (node.right)
    node.right->accept(*this);
}

inline void SecondPassResolver::visit(DerefExpression &node) {
  if (node.right)
    node.right->accept(*this);
}

inline void SecondPassResolver::visit(BinaryExpression &node) {
  if (node.left)
    node.left->accept(*this);
  if (node.right)
    node.right->accept(*this);
}

inline void SecondPassResolver::visit(PrefixExpression &node) {
  if (node.right)
    node.right->accept(*this);
}

inline void SecondPassResolver::visit(ExpressionStatement &node) {
  if (node.expression)
    node.expression->accept(*this);
}

inline void SecondPassResolver::visit(StructExpression &node) {
  for (const auto &field : node.fields) {
    if (field.value)
      field.value.value()->accept(*this);
  }
}

inline void SecondPassResolver::visit(RootNode &) {}

inline ScopeNode *SecondPassResolver::current_scope() const {
  return scope_stack.back();
}

inline void SecondPassResolver::enterScope(ScopeNode *s) {
  if (s) {
    scope_stack.push_back(s);
  }
}

inline void SecondPassResolver::exitScope() {
  if (scope_stack.size() > 1) {
    scope_stack.pop_back();
  }
}

inline SemType SecondPassResolver::resolve_type(LiteralType &t) {
  if (t.is_base()) {
    return SemType::map_primitive(t.as_base());
  }
  if (t.is_tuple()) {
    std::vector<SemType> elems;
    elems.reserve(t.as_tuple().size());
    for (auto &el : t.as_tuple()) {
      elems.push_back(resolve_type(el));
    }
    return SemType::tuple(std::move(elems));
  }
  if (t.is_array()) {
    const auto &arr = t.as_array();
    if (!arr.size) {
      throw SemanticException("array size expression missing");
    }

    auto cv = evaluator.evaluate(arr.size.get(), current_scope());
    if (!cv) {
      throw SemanticException("array size is not a constant expression");
    }

    std::uint64_t size_val = 0;
    if (cv->is_usize()) {
      size_val = cv->as_usize();
    } else if (cv->is_any_int()) {
      auto v = cv->as_any_int();
      if (v < 0)
        throw SemanticException("array size cannot be negative");
      size_val = static_cast<std::uint64_t>(v);
    } else {
      throw SemanticException("array size must be usize");
    }

    t.as_array().actual_size = size_val;

    LOG_DEBUG("[SecondPass] Resolved array size: " + std::to_string(size_val));

    return SemType::array(resolve_type(*arr.element), size_val);
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
    // Qualified paths not supported
    throw SemanticException("qualified path not supported");
  }
  if (t.is_reference()) {
    return SemType::reference(resolve_type(*t.as_reference().target),
                              t.as_reference().is_mutable);
  }
  throw SemanticException("unknown type");
}

inline const CollectedItem *
SecondPassResolver::resolve_named_item(const std::string &name) const {
  for (auto *scope = current_scope(); scope; scope = scope->parent) {
    if (const auto *ci = scope->find_type_item(name)) {
      if (ci->kind == ItemKind::Struct || ci->kind == ItemKind::Enum) {
        return ci;
      }
    }
  }
  return nullptr;
}

inline CollectedItem *
SecondPassResolver::lookup_current_value_item(const std::string &name,
                                              ItemKind kind) {
  auto *scope = current_scope();
  auto *found = scope ? scope->find_value_item(name) : nullptr;
  if (found && found->kind == kind)
    return found;
  throw SemanticException("item " + name + " not found in value namespace");
}

inline CollectedItem *
SecondPassResolver::lookup_current_type_item(const std::string &name,
                                             ItemKind kind) {
  auto *scope = current_scope();
  auto *found = scope ? scope->find_type_item(name) : nullptr;
  if (found && found->kind == kind)
    return found;
  throw SemanticException("item " + name + " not found in type namespace");
}

} // namespace rc