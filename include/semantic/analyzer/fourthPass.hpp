#pragma once

#include <algorithm>
#include <cassert>
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
#include "ast/types.hpp"
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
      throw SemanticException("null expression in evaluate");
    auto it = expr_cache.find(expr);
    if (it != expr_cache.end())
      return it->second;
    expr->accept(*this);
    if (expr_cache.find(expr) == expr_cache.end()) {
      throw SemanticException("expression not cached after evaluation");
    }
    return expr_cache.at(expr);
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
    } else if (auto *expr = dynamic_cast<BorrowExpression *>(&node)) {
      visit(*expr);
    } else if (auto *expr = dynamic_cast<DerefExpression *>(&node)) {
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
      auto *block_expr =
          dynamic_cast<BlockExpression *>(node.body.value().get());
      if (block_expr && block_expr->final_expr) {
        // validate final expression type
        auto ret_typ = expr_cache.at(block_expr->final_expr.value().get());
        if (!can_assign(meta.return_type, ret_typ)) {
          throw TypeError("function '" + node.name +
                          "' return type mismatch: expected '" +
                          to_string(meta.return_type) + "' got '" +
                          to_string(ret_typ) + "'");
        }
      }
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
      if (!can_assign(declared, got)) {
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
    if (!can_assign(annotated, rhs_t)) {
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
    LOG_DEBUG("[FourthPass] Evaluating NameExpression: " + node.name);
    if (auto t = lookup_binding(node.name)) {
      cache_expr(&node, *t);
      return;
    }
    LOG_DEBUG("[FourthPass] NameExpression not in bindings, checking scope");
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

    if (node.op.type == TokenType::AS) {
      handle_as_cast(node);
      return;
    }

    if (!node.left || !node.right) {
      throw SemanticException("binary expression missing operand");
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

    std::optional<SemType> then_t = std::nullopt;
    std::optional<SemType> else_t = std::nullopt;

    if (node.then_block) {
      then_t = evaluate(node.then_block.get());
    }
    if (node.else_block) {
      else_t = evaluate(node.else_block.value().get());
    }

    if (!then_t.has_value() && !else_t.has_value()) {
      throw SemanticException("if expression has no branches");
    }

    if (then_t.has_value() && !else_t.has_value()) {
      if (!can_assign(then_t.value(),
                      SemType::primitive(SemPrimitiveKind::UNIT))) {
        throw TypeError("if expression missing else branch, but then branch "
                        "has non-unit type '" +
                        to_string(then_t.value()) + "'");
      }
      cache_expr(&node, SemType::primitive(SemPrimitiveKind::UNIT));
      return;
    }

    // has then_t and else_t
    if (then_t == else_t) {
      cache_expr(&node, then_t.value());
      return;
    }

    LOG_DEBUG("if expression has type" + to_string(then_t.value()) + " , " +
              to_string(else_t.value()));

    if (auto u = unify_integers(then_t.value(), else_t.value())) {
      cache_expr(&node, *u);
      return;
    }

    throw TypeError(
        "if branches have incompatible types: '" +
        to_string(then_t.value_or(SemType::primitive(SemPrimitiveKind::UNIT))) +
        "' vs '" +
        to_string(else_t.value_or(SemType::primitive(SemPrimitiveKind::UNIT))) +
        "'");
  }

  void visit(MatchExpression &) override {
    throw SemanticException("match expression not supported yet");
  }

  void visit(ReturnExpression &node) override {
    SemType rt = SemType::primitive(SemPrimitiveKind::NEVER);
    if (node.value)
      rt = evaluate(node.value.value().get());
    if (function_return_stack.empty()) {
      throw SemanticException("return outside of function");
    }
    const auto expected = function_return_stack.back();
    if (!can_assign(expected, rt)) {
      throw TypeError("return type mismatch: expected '" + to_string(expected) +
                      "' got '" + to_string(rt) + "'");
    }
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
  }

  void visit(CallExpression &node) override {
    if (auto *nameExpr =
            dynamic_cast<NameExpression *>(node.function_name.get())) {
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
        if (!can_assign(expected, at)) {
          throw TypeError("argument type mismatch in call to " + fn->name +
                          " at position " + std::to_string(i) + ": expected " +
                          to_string(expected) + " got " + to_string(at));
        }
      }

      cache_expr(&node, fn->return_type);
      return;
    } else if (auto *pathExpr =
                   dynamic_cast<PathExpression *>(node.function_name.get())) {
      resolve_path_function_call(*pathExpr, node);
      return;
    }

    throw SemanticException("unsupported call target");
  }

  void visit(MethodCallExpression &node) override {
    auto recv_type = evaluate(node.receiver.get());

    if (recv_type == SemType::primitive(SemPrimitiveKind::ANY_INT)) {
      // TODO: we just UNSAFELY cast it to u32
      recv_type = SemType::primitive(SemPrimitiveKind::U32);
    }

    // Check for builtin methods first
    if (is_builtin_method(recv_type, node.method_name.name)) {
      const size_t argc = node.arguments.size();

      if (argc != 0) {
        throw TypeError("builtin method " + node.method_name.name +
                        " expected 0 arguments, got " + std::to_string(argc));
      }

      auto return_type =
          get_builtin_method_return_type(recv_type, node.method_name.name);
      cache_expr(&node, return_type);
      return;
    }

    if (recv_type.is_reference()) {
      recv_type = auto_deref(recv_type);
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
      if (!can_assign(expected, at)) {
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
    if (target_type.is_reference()) {
      target_type = auto_deref(target_type);
    }
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

  void visit(StructExpression &node) override {
    auto struct_path = dynamic_cast<PathExpression *>(node.path_expr.get());
    if (struct_path) {
      throw SemanticException("struct expressions with path not supported");
    }
    auto struct_name = dynamic_cast<NameExpression *>(node.path_expr.get());
    if (struct_name) {
      CollectedItem *item = nullptr;
      for (auto *s = current_scope_node; s; s = s->parent) {
        if (auto *it = s->find_type_item(struct_name->name)) {
          if (it->kind == ItemKind::Struct && it->has_struct_meta()) {
            item = it;
            break;
          }
        }
      }
      if (!item) {
        throw SemanticException("struct item for '" + struct_name->name +
                                "' not found in scope");
      }
      const auto &meta = item->as_struct_meta();
      if (meta.named_fields.size() != node.fields.size()) {
        throw SemanticException("struct expression field count mismatch");
      }
      for (const auto &f : node.fields) {
        const auto &fname = f.name;
        auto it =
            std::find_if(meta.named_fields.begin(), meta.named_fields.end(),
                         [&fname](const auto &p) { return p.first == fname; });
        if (it == meta.named_fields.end()) {
          throw SemanticException("unknown field '" + fname + "'");
        }

        if (!f.value) {
          throw SemanticException("missing value for field '" + fname + "'");
        }

        auto ft = evaluate(f.value->get());
        if (!can_assign(it->second, ft)) {
          throw TypeError("field '" + fname + "' type mismatch: expected '" +
                          to_string(it->second) + "' got '" + to_string(ft) +
                          "'");
        }
      }
      cache_expr(&node, SemType::named(item));
      return;
    }
    throw SemanticException("struct expressions not found");
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

      LOG_DEBUG("[FourthPass] Successfully added function bindings");
    }

    for (const auto &st : node.statements) {
      if (st)
        st->accept(*this);
    }

    SemType t = SemType::primitive(SemPrimitiveKind::UNIT);
    if (node.final_expr) {
      t = evaluate(node.final_expr.value().get());
    } else {
      if (!node.statements.empty()) {
        if (auto *last_expr = dynamic_cast<ExpressionStatement *>(
                node.statements.back().get())) {
          if (last_expr->expression &&
              dynamic_cast<ReturnExpression *>(last_expr->expression.get())) {
            t = SemType::primitive(SemPrimitiveKind::NEVER);
          }
        }
      }
    }
    cache_expr(&node, t);

    binding_stack.pop_back();
    current_scope_node = saved;
  }

  void visit(LoopExpression &node) override {
    if (node.body)
      (void)evaluate(node.body.get());
    if (loop_break_stack.empty()) {
      throw SemanticException("Why loop without breaks?");
    }
    cache_expr(&node, loop_break_stack.back());
    loop_break_stack.pop_back();
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
      if (node.actual_size < 0) {
        throw SemanticException("ArrayExpr size not resolved");
      }
      cache_expr(&node, SemType::array(el, node.actual_size));
    } else {
      if (node.elements.empty()) {
        cache_expr(&node, SemType::array(
                              SemType::primitive(SemPrimitiveKind::UNIT), 0));
        return;
      }
      auto elem_type = evaluate(node.elements[0].get());
      for (size_t i = 1; i < node.elements.size(); ++i) {
        auto t = evaluate(node.elements[i].get());
        if (t == elem_type)
          continue;
        if (auto u = unify_integers(elem_type, t)) {
          elem_type = *u;
          continue;
        }
        throw TypeError("array elements have inconsistent types");
      }
      cache_expr(&node, SemType::array(elem_type, node.elements.size()));
    }
  }

  void visit(IndexExpression &node) override {
    auto target_t = evaluate(node.target.get());
    auto idx_type = evaluate(node.index.get());
    require_integer(idx_type, "array index must be integer");
    // TODO: validate the array index is in bounds if constant
    // TODO: validate negative index
    if (target_t.is_array() || auto_deref(target_t).is_array()) {
      cache_expr(&node, *auto_deref(target_t).as_array().element);
      return;
    }
    if (target_t.is_slice() || auto_deref(target_t).is_slice()) {
      cache_expr(&node, *auto_deref(target_t).as_slice().element);
      return;
    }
    throw TypeError("indexing non-array/slice type");
  }

  void visit(TupleExpression &) override {}

  void visit(BreakExpression &node) override {
    auto t = SemType::primitive(SemPrimitiveKind::NEVER);
    if (node.expr.has_value()) {
      t = evaluate(node.expr.value().get());
    }
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
    loop_break_stack.push_back(t);
  }

  void visit(ContinueExpression &node) override {
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
  }

  void visit(PathExpression &node) override {
    // as we already handled path function call, so here we handle it as value
    if (node.leading_colons) {
      throw SemanticException("leading colons in paths are not supported");
    }
    if (node.segments.empty()) {
      throw SemanticException("empty path in call");
    }
    for (const auto &seg : node.segments) {
      if (seg.call.has_value()) {
        throw SemanticException("path segment expressions are not supported");
      }
    }

    if (node.segments.size() != 2) {
      throw SemanticException("only TypeName::value calls are supported");
    }

    const std::string &type_name = node.segments[0].ident;
    const std::string &val_name = node.segments[1].ident;

    const CollectedItem *type_item = nullptr;
    for (auto *s = current_scope_node; s; s = s->parent) {
      if (auto *it = s->find_type_item(type_name)) {
        type_item = it;
        break;
      }
    }
    if (!type_item) {
      throw SemanticException("unknown type '" + type_name + "'");
    }
    if (type_item->has_struct_meta()) {
      const auto &meta = type_item->as_struct_meta();

      const ConstantMetaData *found = nullptr;
      for (const auto &m : meta.constants) {
        if (m.name == val_name) {
          found = &m;
          break;
        }
      }
      if (!found) {
        throw TypeError("unknown constant " + val_name);
      }

      cache_expr(&node, found->type);
    } else if (type_item->has_enum_meta()) {
      const auto &meta = type_item->as_enum_meta();

      for (const auto &variant : meta.variant_names) {
        if (variant == val_name) {
          cache_expr(&node, SemType::named(type_item));
          return;
        }
      }
      throw TypeError("unknown enum variant " + val_name);
    } else {
      throw TypeError("type '" + type_name + "' is not a struct or enum");
    }
  }

  void visit(QualifiedPathExpression &) override {
    throw SemanticException("qualified path expression not supported yet");
  }

  void visit(BorrowExpression &node) override {
    auto target_t = evaluate(node.right.get());
    // TODO: validate target_t is mutable and borrowable
    cache_expr(&node, SemType::reference(target_t, node.is_mutable));
  }

  void visit(DerefExpression &node) override {
    auto target_t = evaluate(node.right.get());
    if (!target_t.is_reference()) {
      throw TypeError("cannot deref non-reference type: " +
                      to_string(target_t));
    }
    cache_expr(&node, auto_deref(*target_t.as_reference().target));
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

  std::vector<SemType> loop_break_stack;

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
    } else if (dynamic_cast<const GroupedPattern *>(&pattern)) {
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
      throw SemanticException("cannot create mutable reference pattern "
                              "from immutable reference");
    }
    if (pattern.inner_pattern) {
      extract_pattern_bindings(*pattern.inner_pattern, *ref_type.target);
    }
  }

  bool is_integer(SemPrimitiveKind k) const {
    return k == SemPrimitiveKind::ANY_INT || k == SemPrimitiveKind::I32 ||
           k == SemPrimitiveKind::U32 || k == SemPrimitiveKind::ISIZE ||
           k == SemPrimitiveKind::USIZE;
  }

  bool is_str(SemPrimitiveKind k) const {
    return k == SemPrimitiveKind::STRING;
  }

  void require_bool(const SemType &t, const std::string &msg) {
    if (can_assign(SemType::primitive(SemPrimitiveKind::BOOL), t))
      return;
    throw TypeError(msg);
  }

  void require_integer(const SemType &t, const std::string &msg) {
    if (!(t.is_primitive() && is_integer(t.as_primitive().kind)))
      throw TypeError(msg);
  }

  SemType resolve_type(const LiteralType &type) {
    if (type.is_base()) {
      return SemType::map_primitive(type.as_base());
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
      if (type.as_array().actual_size < 0) {
        throw SemanticException("array size not resolved");
      }
      return SemType::array(resolve_type(*type.as_array().element),
                            type.as_array().actual_size);
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
    if (type.is_reference()) {
      return SemType::reference(resolve_type(*type.as_reference().target),
                                type.as_reference().is_mutable);
    }

    return SemType::primitive(SemPrimitiveKind::UNKNOWN);
  }

  std::optional<SemType> unify_integers(const SemType &a,
                                        const SemType &b) const {
    if (!(a.is_primitive() && b.is_primitive()))
      return std::nullopt;
    auto ak = a.as_primitive().kind;
    auto bk = b.as_primitive().kind;
    if (!(is_integer(ak) && is_integer(bk)))
      return std::nullopt;

    if (a == b)
      return a;
    if (ak == SemPrimitiveKind::ANY_INT && is_integer(bk))
      return b;
    if (bk == SemPrimitiveKind::ANY_INT && is_integer(ak))
      return a;
    return std::nullopt;
  }

  std::optional<SemType> unify_for_op(const SemType &a, const SemType &b,
                                      bool allow_str = false) const {
    if (auto u = unify_integers(a, b))
      return u;
    if (allow_str && a.is_primitive() && b.is_primitive()) {
      auto ak = a.as_primitive().kind;
      auto bk = b.as_primitive().kind;
      if (is_str(ak) && ak == bk)
        return a;
    }
    return std::nullopt;
  }

  bool can_assign(const SemType &dst, const SemType &src) const {
    if (dst == src)
      return true;

    if (dst.is_primitive() && src.is_primitive()) {
      auto dk = dst.as_primitive().kind;
      auto sk = src.as_primitive().kind;
      if (is_integer(dk) && sk == SemPrimitiveKind::ANY_INT)
        return true;
      return false;
    }

    // Array: same length, element also assignable
    if (dst.is_array() && src.is_array()) {
      if (dst.as_array().size != src.as_array().size)
        return false;
      return can_assign(*dst.as_array().element, *src.as_array().element);
    }

    // Slice: element assignable
    if (dst.is_slice() && src.is_slice()) {
      return can_assign(*dst.as_slice().element, *src.as_slice().element);
    }

    // Reference: same mutability, target assignable
    if (dst.is_reference() && src.is_reference()) {
      const auto &dr = dst.as_reference();
      const auto &sr = src.as_reference();
      if (dr.is_mutable && !sr.is_mutable)
        return false;
      return can_assign(*dr.target, *sr.target);
    }

    return false;
  }

  SemType eval_binary(BinaryExpression &bin) {
    auto lt = evaluate(bin.left.get());
    auto rt = evaluate(bin.right.get());
    const auto op = bin.op.type;

    switch (op) {
    case TokenType::PLUS: {
      if (auto r = unify_for_op(lt, rt, true))
        return *r;
      break;
    }
    case TokenType::MINUS:
    case TokenType::STAR:
    case TokenType::SLASH:
    case TokenType::PERCENT: {
      if (auto r = unify_for_op(lt, rt))
        return *r;
      break;
    }
    case TokenType::AMPERSAND:
    case TokenType::PIPE:
    case TokenType::CARET: {
      if (auto r = unify_for_op(lt, rt))
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
      if (lt == rt)
        return SemType::primitive(SemPrimitiveKind::BOOL);
      if (auto u = unify_integers(lt, rt))
        return SemType::primitive(SemPrimitiveKind::BOOL);
      break;
    }
    case TokenType::LT:
    case TokenType::LE:
    case TokenType::GT:
    case TokenType::GE: {
      if (auto u = unify_integers(lt, rt))
        return SemType::primitive(SemPrimitiveKind::BOOL);
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
      throw SemanticException("analyze_place on null expr");

    if (auto *g = dynamic_cast<GroupExpression *>(expr)) {
      LOG_DEBUG("[FourthPass] Analyzing place for GroupExpression");
      return analyze_place(g->inner.get());
    }

    if (auto *n = dynamic_cast<NameExpression *>(expr)) {
      LOG_DEBUG("[FourthPass] Analyzing place for NameExpression: " + n->name);
      PlaceInfo info;
      if (const auto *meta = lookup_binding_meta(n->name)) {
        info.is_place = true;
        info.is_writable = meta->is_mutable;
        info.root_name = n->name;
        return info;
      }

      for (auto *s = current_scope_node; s; s = s->parent) {
        if (s->find_value_item(n->name)) {
          info.is_place = false;
          info.is_writable = false;
          info.root_name = n->name;
          return info;
        }
      }
      throw SemanticException("analyze_place on null expr");
    }

    if (auto *f = dynamic_cast<FieldAccessExpression *>(expr)) {
      LOG_DEBUG("[FourthPass] Analyzing place for FieldAccessExpression: " +
                f->field_name);
      PlaceInfo base = analyze_place(f->target.get());
      if (!base.is_place)
        throw SemanticException("field access target is not a place (base)");
      auto target_t = evaluate(f->target.get());
      bool can_write = base.is_writable;
      if (target_t.is_reference()) {
        can_write = target_t.as_reference().is_mutable;
      }
      target_t = auto_deref(target_t);
      if (!target_t.is_named())
        throw SemanticException("field access target is not named");
      const CollectedItem *ci = target_t.as_named().item;
      if (!ci || !ci->has_struct_meta())
        throw SemanticException("field access target is not a struct");
      bool field_exists = false;
      for (const auto &p : ci->as_struct_meta().named_fields) {
        if (p.first == f->field_name) {
          field_exists = true;
          break;
        }
      }
      if (!field_exists)
        throw SemanticException("field access target is not a struct");
      return PlaceInfo{true, can_write, base.root_name};
    }

    if (auto *idx = dynamic_cast<IndexExpression *>(expr)) {
      LOG_DEBUG("[FourthPass] Analyzing place for IndexExpression");
      PlaceInfo base = analyze_place(idx->target.get());
      if (!base.is_place) {
        LOG_DEBUG("[FourthPass] IndexExpression target is not a place (base)");
        throw SemanticException("IndexExpression target is not a place (base)");
      }
      auto target_t = evaluate(idx->target.get());
      if (!(target_t.is_array() || target_t.is_slice() ||
            auto_deref(target_t).is_array() ||
            auto_deref(target_t).is_slice())) {
        LOG_DEBUG("[FourthPass] IndexExpression target is not array or slice "
                  "(base)");
        throw SemanticException("IndexExpression target is not array or slice "
                                "(base)");
      }

      if (target_t.is_reference()) {
        return PlaceInfo{true, target_t.as_reference().is_mutable,
                         base.root_name};
      }
      return PlaceInfo{true, base.is_writable, base.root_name};
    }

    if (auto *deref = dynamic_cast<DerefExpression *>(expr)) {
      LOG_DEBUG("[FourthPass] Analyzing place for DerefExpression");
      PlaceInfo base = analyze_place(deref->right.get());
      if (!base.is_place)
        throw SemanticException("DerefExpression target is not a place");
      auto target_t = evaluate(deref->right.get());
      if (!target_t.is_reference())
        throw SemanticException("DerefExpression target is not a reference");
      return PlaceInfo{true, target_t.as_reference().is_mutable,
                       base.root_name};
    }

    if (auto *ref = dynamic_cast<BorrowExpression *>(expr)) {
      LOG_DEBUG("[FourthPass] Analyzing place for BorrowExpression");
      PlaceInfo base = analyze_place(ref->right.get());
      if (!base.is_place)
        throw SemanticException("BorrowExpression target is not a place");
      return PlaceInfo{true, ref->is_mutable, base.root_name};
    }

    // All others are r-values
    throw SemanticException("expression is r-value");
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
      throw TypeError("cannot assign, target does not have a writable root");
    }
  }

  void handle_assignment(BinaryExpression &node) {
    require_place_writable(node.left.get(), "assignment");

    auto lhs_t = evaluate(node.left.get());
    auto rhs_t = evaluate(node.right.get());

    if (node.op.type == TokenType::ASSIGN) {
      if (!can_assign(lhs_t, rhs_t)) {
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
    auto match = [&]() -> bool {
      if (!(lhs_is_int && rhs_is_int))
        return false;
      auto rk = rhs_t.as_primitive().kind;
      if (rk == SemPrimitiveKind::ANY_INT)
        return true;
      return lhs_t == rhs_t;
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
      ok = match();
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

  SemType primitive_kind_from_name(Expression *e) {
    auto *ne = dynamic_cast<NameExpression *>(e);
    if (!ne) {
      throw SemanticException("type name must be an identifier");
    }
    const std::string &name = ne->name;
    if (name == "i32")
      return SemType::primitive(SemPrimitiveKind::I32);
    if (name == "u32")
      return SemType::primitive(SemPrimitiveKind::U32);
    if (name == "isize")
      return SemType::primitive(SemPrimitiveKind::ISIZE);
    if (name == "usize")
      return SemType::primitive(SemPrimitiveKind::USIZE);
    if (name == "string")
      return SemType::primitive(SemPrimitiveKind::STRING);
    if (name == "str")
      return SemType::primitive(SemPrimitiveKind::STR);
    if (name == "char")
      return SemType::primitive(SemPrimitiveKind::CHAR);
    if (name == "bool")
      return SemType::primitive(SemPrimitiveKind::BOOL);
    if (name == "never")
      return SemType::primitive(SemPrimitiveKind::NEVER);
    if (name == "unit")
      return SemType::primitive(SemPrimitiveKind::UNIT);

    throw SemanticException("unknown primitive type name '" + name + "'");
  }

  bool is_integer_primitive_kind(SemPrimitiveKind k) const {
    return is_integer(k);
  }

  bool is_integer_type(const SemType &t) const {
    return t.is_primitive() && is_integer_primitive_kind(t.as_primitive().kind);
  }

  bool is_cast_allowed(const SemType &src, const SemType &dst) const {
    if (src == dst)
      return true;
    if (is_integer_type(src) && is_integer_type(dst))
      return true;
    return false;
  }

  void handle_as_cast(BinaryExpression &node) {
    SemType src = evaluate(node.left.get());
    SemType dst = primitive_kind_from_name(node.right.get());

    if (!is_cast_allowed(src, dst)) {
      throw TypeError("invalid cast from '" + to_string(src) + "' to '" +
                      to_string(dst) + "'");
    }
    cache_expr(&node, dst);
  }

  void resolve_path_function_call(const PathExpression &pe,
                                  CallExpression &node) {
    if (pe.leading_colons) {
      throw SemanticException("leading colons in paths are not supported");
    }
    if (pe.segments.empty()) {
      throw SemanticException("empty path in call");
    }
    for (const auto &seg : pe.segments) {
      if (seg.call.has_value()) {
        throw SemanticException("path segment expressions are not supported");
      }
    }

    if (pe.segments.size() != 2) {
      throw SemanticException("only TypeName::function calls are supported");
    }

    const std::string &type_name = pe.segments[0].ident;
    const std::string &fn_name = pe.segments[1].ident;

    const CollectedItem *type_item = nullptr;
    for (auto *s = current_scope_node; s; s = s->parent) {
      if (auto *it = s->find_type_item(type_name)) {
        type_item = it;
        break;
      }
    }
    if (!type_item) {
      throw SemanticException("unknown type '" + type_name + "'");
    }
    if (!type_item->has_struct_meta()) {
      throw TypeError("type '" + type_name + "' is not a struct");
    }

    const auto &meta = type_item->as_struct_meta();

    const FunctionMetaData *found = nullptr;
    for (const auto &m : meta.methods) {
      if (m.name == fn_name) {
        found = &m;
        break;
      }
    }
    if (!found) {
      throw TypeError("unknown method " + fn_name);
    }

    const auto argc = node.arguments.size();
    const auto &params = found->param_types;

    if (params.size() == argc + 1) {
      if (params[0] == SemType::named(type_item)) {
        throw TypeError("cannot call instance method " + fn_name +
                        " as associated function; use method call syntax");
      }
    }

    if (params.size() != argc) {
      throw TypeError(
          "function '" + fn_name + "' argument count mismatch: expected " +
          std::to_string(params.size()) + ", got " + std::to_string(argc));
    }

    for (size_t i = 0; i < argc; ++i) {
      auto at = evaluate(node.arguments[i].get());
      const auto &expected = params[i];
      if (!can_assign(expected, at)) {
        throw TypeError("argument type mismatch in call to " + fn_name +
                        " at position " + std::to_string(i) + ": expected " +
                        to_string(expected) + " got " + to_string(at));
      }
    }

    cache_expr(&node, found->return_type);
  }

  SemType auto_deref(const SemType &t) {
    // TODO: this do not validate `mut`
    if (!t.is_reference())
      return t;
    return auto_deref(*t.as_reference().target);
  }
};

} // namespace rc