#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast/nodes/pattern.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/constEvaluator.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "utils/logger.hpp"

namespace rc {

struct IdentifierMeta {
  bool is_mutable;
  bool is_ref;
  SemType type;
};

class ScopeWithBindings {
public:
  ScopeNode *scope;
  std::map<std::string, IdentifierMeta> bindings; // name -> meta

  ScopeWithBindings(ScopeNode *s) : scope(s) {}

  void add_binding(const std::string &name, const IdentifierMeta &meta) {
    if (scope && scope->find_value_item(name)) {
      // if it is constant, throw
      auto *item = scope->find_value_item(name);
      if (item && item->kind == ItemKind::Constant) {
        throw SemanticException("identifier '" + name +
                                "' conflicts with existing constant in scope");
      }

      // if it is unit struct, throw
      if (item && item->kind == ItemKind::Struct &&
          item->as_struct_meta().named_fields.empty()) {
        throw SemanticException(
            "identifier '" + name +
            "' conflicts with existing unit struct in scope");
      }

      // check enum constructors
      // TODO!
    }

    bindings[name] = meta;
  }
};

class PatternBindingChecker : public BaseVisitor {
public:
  PatternBindingChecker() = default;

  ~PatternBindingChecker() {
    for (auto *scope : allocated_scopes) {
      delete scope;
    }
  }

  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_) {
    LOG_INFO("[PatternBindingChecker] Starting handle of references and bindings");
    root_scope = new ScopeWithBindings(root_scope_);
    scope_stack.clear();
    scope_stack.push_back(root_scope);

    if (root) {
      size_t idx = 0;
      for (const auto &child : root->children) {
        if (child) {
          LOG_DEBUG("[PatternBindingChecker] Evaluating top-level item #" +
                    std::to_string(idx));
          child->accept(*this);
        }
        ++idx;
      }
    }
    LOG_INFO("[PatternBindingChecker] Completed");
  }

  SemType lookup_type(const std::string &name) {
    // First lookup in current scope's bindings
    for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
      auto binding_it = (*it)->bindings.find(name);
      if (binding_it != (*it)->bindings.end()) {
        return binding_it->second.type;
      }
    }

    // Then lookup in the scope tree for items
    for (auto *scope = current_scope()->scope; scope; scope = scope->parent) {
      if (auto *item = scope->find_value_item(name)) {
        switch (item->kind) {
        case ItemKind::Function:
          if (item->has_function_meta()) {
            // maybe return the function return type? dunno
            // TODO: decide what to do here
            return item->as_function_meta().return_type;
          }
          break;
        case ItemKind::Constant:
          if (item->has_constant_meta()) {
            return item->as_constant_meta().type;
          }
          break;
        default:
          break;
        }
      }
    }

    throw SemanticException("identifier '" + name + "' not found");
  }

  void visit(BaseNode &node) override {
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
    } else if (auto *decl = dynamic_cast<ImplDecl *>(&node)) {
      visit(*decl);
    } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
      visit(*expr);
    } else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
      visit(*stmt);
    }
  }

  void visit(ConstantItem &) override {}

  void visit(FunctionDecl &node) override {
    LOG_DEBUG("[PatternBindingChecker] Visiting function '" + node.name + "'");

    if (node.params) {
      auto item = current_scope()->scope->find_value_item(node.name);
      if (!item || item->kind != ItemKind::Function ||
          !item->has_function_meta()) {
        throw SemanticException("function item for '" + node.name +
                                "' not found in scope");
      }
      func_params = {item->as_function_meta().param_names,
                     item->as_function_meta().param_types};
    }

    if (node.body && node.body.value()) {
      node.body.value()->accept(*this);
    }

    func_params = {};
  }

  void visit(LetStatement &node) override {
    LOG_DEBUG("[PatternBindingChecker] Processing let statement");

    SemType binding_type = resolve_type(node.type);
    if (node.type.storage.index() != 0 ||
        !std::holds_alternative<PrimitiveLiteralType>(node.type.storage) ||
        std::get<PrimitiveLiteralType>(node.type.storage) !=
            PrimitiveLiteralType::UNIT) {
      binding_type = resolve_type(node.type);
    }

    if (node.pattern) {
      extract_pattern_bindings(*node.pattern, binding_type);
    }
  }

  void visit(StructDecl &) override {}

  void visit(EnumDecl &) override {}

  void visit(TraitDecl &) override {}

  void visit(ImplDecl &) override {}

  void visit(BlockExpression &node) override {
    LOG_DEBUG("[PatternBindingChecker] Visiting block expression");

    // Find the corresponding scope for this block
    if (auto *block_scope =
            current_scope()->scope->find_child_scope_by_owner(&node)) {
      auto block_bindings = new ScopeWithBindings(block_scope);
      push_scope(block_bindings);

      if (func_params.first.size() != 0) {
        if (func_params.first.size() != func_params.second.size()) {
          throw SemanticException(
              "mismatched function parameter names and types");
        }
        for (size_t i = 0; i < func_params.first.size(); ++i) {
          extract_pattern_bindings(*func_params.first[i], func_params.second[i]);
        }
        func_params = {};
      }

      for (const auto &stmt : node.statements) {
        if (stmt) {
          stmt->accept(*this);
        }
      }

      if (node.final_expr) {
        // it do not create bindings, type check in expr checker.
      }

      pop_scope();
    } else {
      throw SemanticException("block scope not found");
    }
  }

  void visit(IfExpression &node) override {
    LOG_DEBUG("[PatternBindingChecker] Visiting if expression");

    if (node.then_block) {
      node.then_block->accept(*this);
    }

    if (node.else_block) {
      node.else_block.value()->accept(*this);
    }
  }

  void visit(LoopExpression &node) override {
    LOG_DEBUG("[PatternBindingChecker] Visiting loop expression");

    if (node.body) {
      node.body->accept(*this);
    }
  }

  void visit(WhileExpression &node) override {
    LOG_DEBUG("[PatternBindingChecker] Visiting while expression");

    if (node.body) {
      node.body->accept(*this);
    }
  }

  void visit(RootNode &) override {}

private:
  ScopeWithBindings *root_scope = nullptr;
  std::vector<ScopeWithBindings *> scope_stack;
  std::vector<ScopeWithBindings *> allocated_scopes; // For cleanup
  std::pair<std::vector<std::shared_ptr<BasePattern>>, std::vector<SemType>>
      func_params;
  ConstEvaluator evaluator;

  ScopeWithBindings *current_scope() const { return scope_stack.back(); }

  void push_scope(ScopeWithBindings *s) {
    if (s) {
      scope_stack.push_back(s);
      if (s != root_scope) {
        allocated_scopes.push_back(s);
      }
    }
  }

  void pop_scope() {
    if (scope_stack.size() > 1)
      scope_stack.pop_back();
  }

  SemType resolve_type(const LiteralType &type) {
    if (type.is_base()) {
      return SemType::primitive(map_primitive(type.as_base()));
    }
    if (type.is_tuple()) {
      std::vector<SemType> elem_types;
      for (const auto &elem : type.as_tuple()) {
        elem_types.push_back(resolve_type(elem));
      }
      return SemType::tuple(std::move(elem_types));
    }
    if (type.is_array()) {
      return SemType::array(resolve_type(*type.as_array().element),
                            type.as_array().size);
    }
    if (type.is_slice()) {
      return SemType::slice(resolve_type(*type.as_slice().element));
    }
    if (type.is_path()) {
      const auto &segments = type.as_path().segments;
      if (segments.size() == 1) {
        // Look up the named type in the scope tree
        for (auto *scope = current_scope()->scope; scope;
             scope = scope->parent) {
          if (auto *item = scope->find_type_item(segments[0])) {
            return SemType::named(item);
          }
        }
        throw SemanticException("unknown type '" + segments[0] + "'");
      }
      throw SemanticException("qualified paths not supported in types");
    }

    return SemType::primitive(SemPrimitiveKind::UNKNOWN);
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

  void extract_pattern_bindings(const BasePattern &pattern,
                                const SemType &type) {
    if (auto *id_pattern = dynamic_cast<const IdentifierPattern *>(&pattern)) {
      extract_identifier_pattern(*id_pattern, type);
    } else if (auto *ref_pattern =
                   dynamic_cast<const ReferencePattern *>(&pattern)) {
      extract_reference_pattern(*ref_pattern, type);
    } else if (auto *tuple_pattern =
                   dynamic_cast<const TuplePattern *>(&pattern)) {
      extract_tuple_pattern(*tuple_pattern, type);
    } else if (auto *struct_pattern =
                   dynamic_cast<const StructPattern *>(&pattern)) {
      extract_struct_pattern(*struct_pattern, type);
    } else if (auto *grouped_pattern =
                   dynamic_cast<const GroupedPattern *>(&pattern)) {
      extract_pattern_bindings(*grouped_pattern->inner_pattern, type);
    } else if (auto *or_pattern = dynamic_cast<const OrPattern *>(&pattern)) {
      // Or pattern is removed
    }
    // LiteralPattern, WildcardPattern, don't create bindings
  }

  void extract_identifier_pattern(const IdentifierPattern &pattern,
                                  const SemType &type) {
    IdentifierMeta meta{pattern.is_mutable, pattern.is_ref, type};
    LOG_DEBUG("[PatternBindingChecker] Adding binding: " + pattern.name +
              (pattern.is_mutable ? " (mut)" : "") +
              (pattern.is_ref ? " (ref)" : ""));
    current_scope()->add_binding(pattern.name, meta);
  }

  void extract_reference_pattern(const ReferencePattern &pattern,
                                 const SemType &type) {
    if (!type.is_reference()) {
      throw SemanticException(
          "reference pattern requires a reference type, got " +
          to_string(type));
    }

    const auto &ref_type = type.as_reference();

    if (pattern.is_mutable && !ref_type.is_mutable) {
      throw SemanticException(
          "cannot create mutable reference pattern from immutable reference");
    }

    if (pattern.inner_pattern) {
      extract_pattern_bindings(*pattern.inner_pattern, *ref_type.target);
    }
  }

  void extract_tuple_pattern(const TuplePattern &, const SemType &) {
    // Tuple pattern is removed.
    throw SemanticException("tuple patterns is removed");
  }

  void extract_struct_pattern(const StructPattern &, const SemType &) {
    // Struct pattern is removed.
    throw SemanticException("struct patterns is removed");
  }
};

} // namespace rc
