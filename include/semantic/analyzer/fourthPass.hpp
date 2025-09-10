#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/pattern.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "semantic/analyzer/builtin.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "utils/logger.hpp"

namespace rc {

class FourthPass : public BaseVisitor {
public:
  FourthPass() = default;
  ~FourthPass() = default;

  // Entry point
  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_) {
    if (!root || !root_scope_) {
      throw SemanticException("FourthPass: null root or root scope");
    }
    LOG_INFO("[FourthPass] Starting semantic analysis");
    current_scope_node = root_scope_;
    binding_stack.clear();
    binding_stack.emplace_back();

    for (const auto &child : root->children) {
      if (child)
        child->accept(*this);
    }
    LOG_INFO("[FourthPass] Completed");
  }

  SemType evaluate(Expression *expr) {
    if (!expr)
      return SemType::primitive(SemPrimitiveKind::UNIT);
    auto it = expr_cache.find(expr);
    if (it != expr_cache.end())
      return it->second;
    expr->accept(*this);
    return expr_cache.at(expr);
  }

  const std::unordered_map<const BaseNode *, SemType> &results() const {
    return expr_cache;
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
    } else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
      visit(*stmt);
    } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
      visit(*stmt);
    } else if (auto *stmt = dynamic_cast<EmptyStatement *>(&node)) {
      visit(*stmt);
    } else if (auto *expr = dynamic_cast<NameExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<LiteralExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<PrefixExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<BinaryExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<GroupExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<MatchExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<ReturnExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<CallExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<MethodCallExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<FieldAccessExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<StructExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<UnderscoreExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<ArrayExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<IndexExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<TupleExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<BreakExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<ContinueExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<PathExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<QualifiedPathExpression *>(&node)) {
      visit(*expr);
    }
  }

  void visit(RootNode &node) override {
    for (auto &c : node.children) {
      if (c)
        c->accept(*this);
    }
  }

  void visit(FunctionDecl &node) override {
    LOG_DEBUG("[FourthPass] Visiting function '" + node.name + "'");
    auto *item = current_scope_node->find_value_item(node.name);
    if (!item || item->kind != ItemKind::Function ||
        !item->has_function_meta()) {
      throw SemanticException("function item for '" + node.name +
                              "' not found in scope");
    }
    const auto &meta = item->as_function_meta();

    function_return_stack.push_back(meta.return_type);

    if (node.body && node.body.value()) {
      pending_params = std::make_pair(meta.param_names, meta.param_types);
      node.body.value()->accept(*this);
      pending_params.reset();
    }

    function_return_stack.pop_back();
  }

  void visit(ConstantItem &node) override {
    if (node.value) {
      auto *item = current_scope_node->find_value_item(node.name);
      if (!item || item->kind != ItemKind::Constant ||
          !item->has_constant_meta()) {
        throw SemanticException("constant item for '" + node.name +
                                "' not found in scope");
      }
      const auto &meta = item->as_constant_meta();
      SemType declared = meta.type;
      auto got = evaluate(node.value.value().get());
      if (!(got == declared)) {
        throw TypeError("constant '" + node.name +
                        "' type mismatch: expected '" + to_string(declared) +
                        "' got '" + to_string(got) + "'");
      }
    }
  }

  void visit(StructDecl &) override {}
  void visit(EnumDecl &) override {}
  void visit(TraitDecl &) override {}
  void visit(ImplDecl &) override {}

  void visit(LetStatement &node) override {
    LOG_DEBUG("[FourthPass] Processing let statement");
    SemType annotated = resolve_type(node.type);

    if (!node.expr) {
      // TODO: allow uninitialized let
      throw SemanticException("let initializer expression is required");
    }
    auto rhs_t = evaluate(node.expr.get());
    if (!(rhs_t == annotated)) {
      throw TypeError("let initializer type mismatch: expected '" +
                      to_string(annotated) + "' got '" + to_string(rhs_t) +
                      "'");
    }

    if (node.pattern) {
      validate_irrefutable_pattern(*node.pattern);
      extract_pattern_bindings(*node.pattern, annotated);
    } else {
      throw SemanticException("let pattern is required");
    }
  }

  void visit(ExpressionStatement &node) override {
    if (node.expression)
      (void)evaluate(node.expression.get());
  }

  void visit(EmptyStatement &) override {}

  void visit(NameExpression &node) override {
    if (auto t = lookup_binding(node.name)) {
      cache_expr(&node, *t);
      return;
    }
    // fall back to scope value items
    for (auto *s = current_scope_node; s; s = s->parent) {
      if (auto *it = s->find_value_item(node.name)) {
        if (it->kind == ItemKind::Constant && it->has_constant_meta()) {
          cache_expr(&node, it->as_constant_meta().type);
          return;
        }
        if (it->kind == ItemKind::Function) {
          throw SemanticException("function name '" + node.name +
                                  "' used as value");
        }
        if (it->kind == ItemKind::Struct) {
          throw SemanticException("struct '" + node.name + "' used as value");
        }
      }
    }
    throw SemanticException("identifier '" + node.name + "' not found");
  }

  void visit(LiteralExpression &node) override {
    cache_expr(&node, resolve_type(node.type));
  }

  void visit(PrefixExpression &node) override {
    auto rt = evaluate(node.right.get());
    switch (node.op.type) {
    case TokenType::NOT:
      require_bool(rt, "operator ! requires bool operand");
      cache_expr(&node, SemType::primitive(SemPrimitiveKind::BOOL));
      break;
    case TokenType::MINUS:
      require_integer(rt, "unary - requires integer operand");
      cache_expr(&node, rt);
      break;
    default:
      throw TypeError("unsupported prefix operator");
    }
  }

  void visit(BinaryExpression &node) override {
    if (is_assignment_token(node.op.type)) {
      handle_assignment(node);
      return;
    }

    cache_expr(&node, eval_binary(node));
  }

  void visit(GroupExpression &node) override {
    cache_expr(&node, evaluate(node.inner.get()));
  }

  void visit(IfExpression &node) override {
    if (node.condition) {
      auto ct = evaluate(node.condition.get());
      require_bool(ct, "if condition must be bool");
    }
    auto then_t = node.then_block ? evaluate(node.then_block.get())
                                  : SemType::primitive(SemPrimitiveKind::UNIT);
    auto else_t = node.else_block ? evaluate(node.else_block.value().get())
                                  : SemType::primitive(SemPrimitiveKind::UNIT);
    if (!(then_t == else_t)) {
      throw TypeError("if branches have incompatible types: '" +
                      to_string(then_t) + "' vs '" + to_string(else_t) + "'");
    }
    cache_expr(&node, then_t);
  }

  void visit(MatchExpression &) override {
    throw SemanticException("match expression not supported yet");
  }

  void visit(ReturnExpression &node) override {
    SemType rt = SemType::primitive(SemPrimitiveKind::UNIT);
    if (node.value)
      rt = evaluate(node.value.value().get());
    if (function_return_stack.empty()) {
      throw SemanticException("return outside of function");
    }
    const auto expected = function_return_stack.back();
    if (!(rt == expected)) {
      throw TypeError("return type mismatch: expected '" + to_string(expected) +
                      "' got '" + to_string(rt) + "'");
    }
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
  }

  void visit(CallExpression &node) override {
    auto *nameExpr = dynamic_cast<NameExpression *>(node.function_name.get());
    if (!nameExpr) {
      throw SemanticException("call target must be a function name");
    }

    const FunctionMetaData *fn = nullptr;
    for (auto *s = current_scope_node; s; s = s->parent) {
      if (auto *it = s->find_value_item(nameExpr->name)) {
        if (it->kind == ItemKind::Function && it->has_function_meta()) {
          fn = &it->as_function_meta();
          break;
        }
      }
    }
    if (!fn) {
      throw SemanticException("unknown function '" + nameExpr->name + "'");
    }

    const size_t argc = node.arguments.size();
    if (fn->param_types.size() != argc) {
      throw TypeError("function '" + fn->name +
                      "' argument count mismatch: expected " +
                      std::to_string(fn->param_types.size()) + ", got " +
                      std::to_string(argc));
    }

    for (size_t i = 0; i < argc; ++i) {
      auto at = evaluate(node.arguments[i].get());
      const auto &expected = fn->param_types[i];
      if (!(at == expected)) {
        throw TypeError("argument type mismatch in call to " + fn->name +
                        " at position " + std::to_string(i) + ": expected " +
                        to_string(expected) + " got " + to_string(at));
      }
    }

    cache_expr(&node, fn->return_type);
  }

  void visit(MethodCallExpression &node) override {
    auto recv_type = evaluate(node.receiver.get());
    
    // Check for builtin methods first
    if (is_builtin_method(recv_type, node.method_name.name)) {
      const size_t argc = node.arguments.size();

      if (argc != 0) {
        throw TypeError(
          "builtin method " + node.method_name.name + " expected 0 arguments, got " + std::to_string(argc));
      }
      
      auto return_type = get_builtin_method_return_type(recv_type, node.method_name.name);
      cache_expr(&node, return_type);
      return;
    }
    
    if (!recv_type.is_named()) {
      throw TypeError("method call on non-struct type: " +
                      to_string(recv_type));
    }

    const CollectedItem *ci = recv_type.as_named().item;
    if (!ci || !ci->has_struct_meta()) {
      throw TypeError("method call target is not a struct");
    }

    const auto &meta = ci->as_struct_meta();
    const FunctionMetaData *found = nullptr;
    for (const auto &m : meta.methods) {
      if (m.name == node.method_name.name) {
        found = &m;
        break;
      }
    }
    if (!found) {
      throw TypeError("unknown method " + node.method_name.name);
    }

    const auto &params = found->param_types;
    const size_t argc = node.arguments.size();
    if (!(params.size() == argc || params.size() == argc + 1)) {
      throw TypeError(
          "method " + node.method_name.name + " signature mismatch: expected " +
          std::to_string(params.size()) + ", got " + std::to_string(argc + 1));
    }

    size_t arg_index_offset = 0;
    if (params.size() == argc + 1) {
      // Method with self parameter
      if (!(recv_type == params[0])) {
        throw TypeError("receiver type mismatch for method " +
                        node.method_name.name + ": expected " +
                        to_string(params[0]) + " got " + to_string(recv_type));
      }
      arg_index_offset = 1;
    }
    for (size_t i = 0; i < argc; ++i) {
      auto at = evaluate(node.arguments[i].get());
      const auto &expected = params[i + arg_index_offset];
      if (!(at == expected)) {
        throw TypeError("argument type mismatch in method " +
                        node.method_name.name + " at position " +
                        std::to_string(i) + ": expected " +
                        to_string(expected) + " got " + to_string(at));
      }
    }
    cache_expr(&node, found->return_type);
  }

  void visit(FieldAccessExpression &node) override {
    auto target_type = evaluate(node.target.get());

    bool numeric = !node.field_name.empty() &&
                   std::all_of(node.field_name.begin(), node.field_name.end(),
                               [](unsigned char c) { return std::isdigit(c); });
    if (numeric) {
      throw SemanticException("tuple field access not supported yet");
    }
    // Struct field access
    if (!target_type.is_named()) {
      throw TypeError("field access on non-struct type '" +
                      to_string(target_type) + "'");
    }
    const CollectedItem *ci = target_type.as_named().item;
    if (!ci || !ci->has_struct_meta()) {
      throw TypeError("field access target is not a struct");
    }
    const auto &meta = ci->as_struct_meta();
    for (const auto &f : meta.named_fields) {
      if (f.first == node.field_name) {
        cache_expr(&node, f.second);
        return;
      }
    }
    throw TypeError("unknown field '" + node.field_name + "'");
  }

  void visit(StructExpression &) override {
    throw SemanticException("struct literal not supported yet");
  }

  void visit(UnderscoreExpression &node) override {
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::UNIT));
  }

  void visit(BlockExpression &node) override {
    auto *block_scope = current_scope_node->find_child_scope_by_owner(&node);
    if (!block_scope) {
      throw SemanticException("block scope not found");
    }
    ScopeNode *saved = current_scope_node;
    current_scope_node = const_cast<ScopeNode *>(block_scope);

    binding_stack.emplace_back();

    if (pending_params) {
      const auto &names = pending_params->first;
      const auto &types = pending_params->second;
      if (names.size() != types.size()) {
        throw SemanticException(
            "mismatched function parameter names and types");
      }
      for (size_t i = 0; i < names.size(); ++i) {
        validate_irrefutable_pattern(*names[i]);
        extract_pattern_bindings(*names[i], types[i]);
      }
      pending_params.reset();
    }

    for (const auto &st : node.statements) {
      if (st)
        st->accept(*this);
    }

    SemType t = SemType::primitive(SemPrimitiveKind::UNIT);
    if (node.final_expr) {
      t = evaluate(node.final_expr.value().get());
    }
    cache_expr(&node, t);

    binding_stack.pop_back();
    current_scope_node = saved;
  }

  void visit(LoopExpression &node) override {
    if (node.body)
      (void)evaluate(node.body.get());
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::UNIT));
  }

  void visit(WhileExpression &node) override {
    if (node.condition) {
      auto ct = evaluate(node.condition.get());
      require_bool(ct, "while condition must be bool");
    }
    if (node.body)
      (void)evaluate(node.body.get());
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::UNIT));
  }

  void visit(ArrayExpression &node) override {
    if (node.repeat) {
      auto el = evaluate(node.repeat->first.get());
      // actually, we removed repeat here.
      // cache_expr(&node, SemType::array(el, 0));
      throw SemanticException("array repeat syntax removed");
    } else {
      if (node.elements.empty()) {
        cache_expr(&node, SemType::array(
                              SemType::primitive(SemPrimitiveKind::UNIT), 0));
        return;
      }
      auto first = evaluate(node.elements[0].get());
      for (size_t i = 1; i < node.elements.size(); ++i) {
        auto t = evaluate(node.elements[i].get());
        if (!(t == first)) {
          throw TypeError("array elements have inconsistent types");
        }
      }
      cache_expr(&node, SemType::array(first, node.elements.size()));
    }
  }

  void visit(IndexExpression &node) override {
    auto target_t = evaluate(node.target.get());
    auto idx_type = evaluate(node.index.get());
    require_integer(idx_type, "array index must be integer");
    // TODO: validate the array index is in bounds if constant
    // TODO: validate negative index
    if (target_t.is_array()) {
      cache_expr(&node, *target_t.as_array().element);
      return;
    }
    if (target_t.is_slice()) {
      cache_expr(&node, *target_t.as_slice().element);
      return;
    }
    throw TypeError("indexing non-array/slice type");
  }

  void visit(TupleExpression &) override {}

  void visit(BreakExpression &node) override {
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
  }

  void visit(ContinueExpression &node) override {
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
  }

  void visit(PathExpression &) override {}

  void visit(QualifiedPathExpression &) override {
    throw SemanticException("qualified path expression not supported yet");
  }

private:
  struct IdentifierMeta {
    bool is_mutable;
    bool is_ref;
    SemType type;
  };

  struct PlaceInfo {
    bool is_place = false;
    bool is_writable = false;
    std::optional<std::string> root_name;
  };

  using BindingFrame = std::map<std::string, IdentifierMeta>;
  std::vector<BindingFrame> binding_stack;

  std::optional<std::pair<std::vector<std::shared_ptr<BasePattern>>,
                          std::vector<SemType>>>
      pending_params;

  ScopeNode *current_scope_node = nullptr;

  std::unordered_map<const BaseNode *, SemType> expr_cache;

  std::vector<SemType> function_return_stack;

  void cache_expr(const BaseNode *n, SemType t) {
    if (!n)
      return;
    auto it = expr_cache.find(n);
    if (it == expr_cache.end()) {
      expr_cache.emplace(n, std::move(t));
    } else {
      throw SemanticException("revisit the same expr again, why?");
    }
  }

  std::optional<SemType> lookup_binding(const std::string &name) const {
    for (auto it = binding_stack.rbegin(); it != binding_stack.rend(); ++it) {
      auto found = it->find(name);
      if (found != it->end())
        return found->second.type;
    }
    return std::nullopt;
  }

  const IdentifierMeta *lookup_binding_meta(const std::string &name) const {
    for (auto it = binding_stack.rbegin(); it != binding_stack.rend(); ++it) {
      auto found = it->find(name);
      if (found != it->end())
        return &found->second;
    }
    return nullptr;
  }

  void add_binding(const std::string &name, const IdentifierMeta &meta) {
    if (current_scope_node) {
      auto *item = current_scope_node->find_value_item(name);
      if (item) {
        if (item->kind == ItemKind::Constant) {
          throw SemanticException(
              "identifier '" + name +
              "' conflicts with existing constant in scope");
        }
        if (item->kind == ItemKind::Struct && item->has_struct_meta() &&
            item->as_struct_meta().named_fields.empty()) {
          // unit struct constructor in value namespace
          throw SemanticException(
              "identifier '" + name +
              "' conflicts with existing unit struct in scope");
        }
        // TODO: enum constructor collisions if exposed in value namespace
      }
    }

    if (binding_stack.empty())
      throw SemanticException("internal error: empty binding stack");
    (*binding_stack.rbegin())[name] = meta;
  }

  // pattern binding
  void extract_pattern_bindings(const BasePattern &pattern,
                                const SemType &type) {
    if (auto *idp = dynamic_cast<const IdentifierPattern *>(&pattern)) {
      extract_identifier_pattern(*idp, type);
      return;
    } else if (auto *refp = dynamic_cast<const ReferencePattern *>(&pattern)) {
      extract_reference_pattern(*refp, type);
      return;
    } else if (dynamic_cast<const TuplePattern *>(&pattern)) {
      throw SemanticException("tuple patterns is removed");
    } else if (dynamic_cast<const StructPattern *>(&pattern)) {
      throw SemanticException("struct patterns is removed");
    } else if (auto *grp = dynamic_cast<const GroupedPattern *>(&pattern)) {
      throw SemanticException("grouped patterns is removed");
    } else if (dynamic_cast<const OrPattern *>(&pattern)) {
      // or pattern removed in subset
      return;
    }
    // LiteralPattern, WildcardPattern: do not create bindings
  }

  void extract_identifier_pattern(const IdentifierPattern &pattern,
                                  const SemType &type) {
    IdentifierMeta meta{pattern.is_mutable, pattern.is_ref, type};
    LOG_DEBUG("[FourthPass] Adding binding: " + pattern.name +
              (pattern.is_mutable ? " (mut)" : "") +
              (pattern.is_ref ? " (ref)" : ""));
    add_binding(pattern.name, meta);
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

  bool is_integer(SemPrimitiveKind k) const {
    return k == SemPrimitiveKind::I32 || k == SemPrimitiveKind::U32 ||
           k == SemPrimitiveKind::ISIZE || k == SemPrimitiveKind::USIZE;
  }

  bool is_str(SemPrimitiveKind k) const {
    return k == SemPrimitiveKind::STRING || k == SemPrimitiveKind::RAW_STRING ||
           k == SemPrimitiveKind::C_STRING ||
           k == SemPrimitiveKind::RAW_C_STRING;
  }

  void require_bool(const SemType &t, const std::string &msg) {
    if (!(t.is_primitive() && t.as_primitive().kind == SemPrimitiveKind::BOOL))
      throw TypeError(msg);
  }

  void require_integer(const SemType &t, const std::string &msg) {
    if (!(t.is_primitive() && is_integer(t.as_primitive().kind)))
      throw TypeError(msg);
  }

  SemType resolve_type(const LiteralType &type) {
    if (type.is_base()) {
      return SemType::primitive(map_primitive(type.as_base()));
    }
    if (type.is_tuple()) {
      std::vector<SemType> elem_types;
      elem_types.reserve(type.as_tuple().size());
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
        for (auto *scope = current_scope_node; scope; scope = scope->parent) {
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

  SemType eval_binary(BinaryExpression &bin) {
    auto lt = evaluate(bin.left.get());
    auto rt = evaluate(bin.right.get());
    const auto op = bin.op.type;

    bool left_literal = is_plain_i32_literal(bin.left.get());
    bool right_literal = is_plain_i32_literal(bin.right.get());

    auto check = [&](bool allow_str = false) -> std::optional<SemType> {
      if (lt.is_primitive() && rt.is_primitive()) {
        auto lk = lt.as_primitive().kind;
        auto rk = rt.as_primitive().kind;
        if (is_integer(lk) && is_integer(rk)) {
          if (lk == rk && (lt == rt))
            return lt;
          if (left_literal && is_integer(rk))
            return rt;
          if (right_literal && is_integer(lk))
            return lt;
        }
        if (allow_str && is_str(lk) && lk == rk)
          return lt;
      }
      return std::nullopt;
    };

    switch (op) {
    case TokenType::PLUS: {
      if (auto r = check(true))
        return *r;
      break;
    }
    case TokenType::MINUS:
    case TokenType::STAR:
    case TokenType::SLASH:
    case TokenType::PERCENT: {
      if (auto r = check())
        return *r;
      break;
    }
    case TokenType::AMPERSAND:
    case TokenType::PIPE:
    case TokenType::CARET: {
      if (auto r = check())
        return *r;
      break;
    }
    case TokenType::SHL:
    case TokenType::SHR: {
      if (lt.is_primitive() && is_integer(lt.as_primitive().kind) &&
          rt.is_primitive() && is_integer(rt.as_primitive().kind))
        return lt;
      break;
    }
    case TokenType::AND:
    case TokenType::OR: {
      if (lt.is_primitive() && rt.is_primitive() &&
          lt.as_primitive().kind == SemPrimitiveKind::BOOL &&
          rt.as_primitive().kind == SemPrimitiveKind::BOOL)
        return SemType::primitive(SemPrimitiveKind::BOOL);
      break;
    }
    case TokenType::EQ:
    case TokenType::NE: {
      if (lt == rt || (lt.is_primitive() && rt.is_primitive() &&
                       is_integer(lt.as_primitive().kind) &&
                       is_integer(rt.as_primitive().kind) &&
                       (left_literal || right_literal)))
        return SemType::primitive(SemPrimitiveKind::BOOL);
      break;
    }
    case TokenType::LT:
    case TokenType::LE:
    case TokenType::GT:
    case TokenType::GE: {
      if (lt.is_primitive() && rt.is_primitive() &&
          is_integer(lt.as_primitive().kind) &&
          is_integer(rt.as_primitive().kind)) {
        if (lt == rt || left_literal || right_literal)
          return SemType::primitive(SemPrimitiveKind::BOOL);
      }
      break;
    }
    default:
      throw TypeError("unsupported binary operator");
    }

    throw TypeError("operator '" + std::string(toString(op)) +
                    "' not applicable to types '" + to_string(lt) + "' and '" +
                    to_string(rt) + "'");
  }

  bool is_assignment_token(TokenType tt) const {
    switch (tt) {
    case TokenType::ASSIGN:
    case TokenType::PLUS_EQ:
    case TokenType::MINUS_EQ:
    case TokenType::STAR_EQ:
    case TokenType::SLASH_EQ:
    case TokenType::PERCENT_EQ:
    case TokenType::AMPERSAND_EQ:
    case TokenType::PIPE_EQ:
    case TokenType::CARET_EQ:
    case TokenType::SHL_EQ:
    case TokenType::SHR_EQ:
      return true;
    default:
      return false;
    }
  }

  std::optional<TokenType> compound_base_operator(TokenType tt) const {
    switch (tt) {
    case TokenType::PLUS_EQ:
      return TokenType::PLUS;
    case TokenType::MINUS_EQ:
      return TokenType::MINUS;
    case TokenType::STAR_EQ:
      return TokenType::STAR;
    case TokenType::SLASH_EQ:
      return TokenType::SLASH;
    case TokenType::PERCENT_EQ:
      return TokenType::PERCENT;
    case TokenType::AMPERSAND_EQ:
      return TokenType::AMPERSAND;
    case TokenType::PIPE_EQ:
      return TokenType::PIPE;
    case TokenType::CARET_EQ:
      return TokenType::CARET;
    case TokenType::SHL_EQ:
      return TokenType::SHL;
    case TokenType::SHR_EQ:
      return TokenType::SHR;
    default:
      return std::nullopt;
    }
  }

  PlaceInfo analyze_place(Expression *expr) {
    if (!expr)
      return {};

    if (auto *g = dynamic_cast<GroupExpression *>(expr)) {
      return analyze_place(g->inner.get());
    }

    if (auto *n = dynamic_cast<NameExpression *>(expr)) {
      PlaceInfo info;
      if (const auto *meta = lookup_binding_meta(n->name)) {
        info.is_place = true;
        info.is_writable = meta->is_mutable;
        info.root_name = n->name;
        return info;
      }

      for (auto *s = current_scope_node; s; s = s->parent) {
        if (auto *it = s->find_value_item(n->name)) {
          info.is_place = false;
          info.is_writable = false;
          info.root_name = n->name;
          return info;
        }
      }
      return {};
    }

    if (auto *f = dynamic_cast<FieldAccessExpression *>(expr)) {
      PlaceInfo base = analyze_place(f->target.get());
      if (!base.is_place)
        return {};
      auto target_t = evaluate(f->target.get());
      if (!target_t.is_named())
        return {};
      const CollectedItem *ci = target_t.as_named().item;
      if (!ci || !ci->has_struct_meta())
        return {};
      bool field_exists = false;
      for (const auto &p : ci->as_struct_meta().named_fields) {
        if (p.first == f->field_name) {
          field_exists = true;
          break;
        }
      }
      if (!field_exists)
        return {};
      return PlaceInfo{true, base.is_writable, base.root_name};
    }

    if (auto *idx = dynamic_cast<IndexExpression *>(expr)) {
      PlaceInfo base = analyze_place(idx->target.get());
      if (!base.is_place)
        return {};
      auto target_t = evaluate(idx->target.get());
      if (!(target_t.is_array() || target_t.is_slice()))
        return {};
      return PlaceInfo{true, base.is_writable, base.root_name};
    }

    // All others are r-values
    return {};
  }

  void require_place_writable(Expression *lhs, const char *context) {
    PlaceInfo info = analyze_place(lhs);
    if (!info.is_place) {
      throw TypeError(std::string(context) + ": invalid left-hand side");
    }
    if (!info.is_writable) {
      if (info.root_name) {
        throw TypeError(std::string(context) +
                        ": cannot assign to immutable binding '" +
                        *info.root_name + "'");
      }
      throw TypeError(std::string(context) +
                      ": cannot assign to immutable place");
    }
  }

  bool is_plain_i32_literal(Expression *e) const {
    auto *lit = dynamic_cast<LiteralExpression *>(e);
    if (!lit)
      return false;
    if (!(lit->type.is_base() &&
          lit->type.as_base() == PrimitiveLiteralType::I32)) {
      // it is due to in parser we assume bare int literals to be i32 (no
      // suffix)
      // TODO: maybe we need a fix here.
      return false;
    }
    static const std::array<std::string, 4> suffixes = {"i32", "u32", "isize",
                                                        "usize"};
    for (const auto &s : suffixes) {
      if (lit->value.size() >= s.size() &&
          lit->value.compare(lit->value.size() - s.size(), s.size(), s) == 0) {
        return false;
      }
    }
    return true;
  }

  void handle_assignment(BinaryExpression &node) {
    require_place_writable(node.left.get(), "assignment");

    auto lhs_t = evaluate(node.left.get());
    auto rhs_t = evaluate(node.right.get());

    if (node.op.type == TokenType::ASSIGN) {
      if (!(lhs_t == rhs_t)) {
        throw TypeError("assignment type mismatch: expected '" +
                        to_string(lhs_t) + "', got '" + to_string(rhs_t) + "'");
      }
      cache_expr(&node, SemType::primitive(SemPrimitiveKind::UNIT));
      return;
    }

    auto base_op_opt = compound_base_operator(node.op.type);
    if (!base_op_opt) {
      throw TypeError("unsupported compound assignment");
    }
    const auto base_op = *base_op_opt;

    bool lhs_is_int =
        lhs_t.is_primitive() && is_integer(lhs_t.as_primitive().kind);
    bool rhs_is_int =
        rhs_t.is_primitive() && is_integer(rhs_t.as_primitive().kind);
    bool right_literal = is_plain_i32_literal(node.right.get());

    auto unify_int = [&]() -> bool {
      if (!(lhs_is_int && rhs_is_int))
        return false;
      if (lhs_t == rhs_t)
        return true;
      if (right_literal)
        return true; // allow literal to adopt lhs type
      return false;
    };

    bool ok = false;
    switch (base_op) {
    case TokenType::PLUS:
    case TokenType::MINUS:
    case TokenType::STAR:
    case TokenType::SLASH:
    case TokenType::PERCENT:
    case TokenType::AMPERSAND:
    case TokenType::PIPE:
    case TokenType::CARET:
      ok = unify_int();
      break;
    case TokenType::SHL:
    case TokenType::SHR:
      ok = lhs_is_int && rhs_is_int; // shift keeps lhs type
      break;
    default:
      ok = false;
    }

    if (!ok) {
      throw TypeError("invalid types for compound assignment: '" +
                      to_string(lhs_t) + "' " + std::string(toString(base_op)) +
                      "= '" + to_string(rhs_t) + "'");
    }

    // Assignment expression type is unit
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::UNIT));
  }

  void validate_irrefutable_pattern(const BasePattern &pattern) {
    if (auto *idp = dynamic_cast<const IdentifierPattern *>(&pattern)) {
      (void)idp; // ok
      return;
    }
    if (auto *grp = dynamic_cast<const GroupedPattern *>(&pattern)) {
      if (grp->inner_pattern) {
        validate_irrefutable_pattern(*grp->inner_pattern);
      }
      return;
    }
    if (auto *refp = dynamic_cast<const ReferencePattern *>(&pattern)) {
      if (refp->inner_pattern) {
        validate_irrefutable_pattern(*refp->inner_pattern);
      }
      return;
    }
    if (dynamic_cast<const WildcardPattern *>(&pattern)) {
      return; // ok
    }

    if (dynamic_cast<const LiteralPattern *>(&pattern)) {
      throw SemanticException("literal pattern is not allowed");
    }
    if (dynamic_cast<const OrPattern *>(&pattern)) {
      throw SemanticException("or patterns is removed");
    }
    if (dynamic_cast<const TuplePattern *>(&pattern)) {
      throw SemanticException("tuple patterns is removed");
    }
    if (dynamic_cast<const StructPattern *>(&pattern)) {
      throw SemanticException("struct patterns is removed");
    }

    throw SemanticException("unsupported pattern");
  }
};

} // namespace rc