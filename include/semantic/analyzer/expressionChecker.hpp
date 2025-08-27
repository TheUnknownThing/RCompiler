#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/topLevel.hpp"
#include "ast/types.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "utils/logger.hpp"

namespace rc {

class ExpressionChecker : public BaseVisitor {
public:
  ExpressionChecker() = delete;

  explicit ExpressionChecker(std::function<SemType(const std::string &)> lookup,
                             std::function<SemType(LiteralType)> resolve_type)
      : lookup(lookup), resolve_type(resolve_type) {}

  SemType evaluate(Expression *expr) {
    if (!expr)
      return SemType::primitive(SemPrimitiveKind::UNIT);
    auto it = cache.find(expr);
    if (it != cache.end())
      return it->second;
    expr->accept(*this);
    return cache.at(expr);
  }

  void visit(NameExpression &node) override {
    cache_expr(&node, lookup(node.name));
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
    if (function_return_types.empty()) {
      throw SemanticException("return outside of function");
    }
    const auto expected = function_return_types.back();
    if (!(rt == expected)) {
      throw TypeError("return type mismatch: expected '" + to_string(expected) +
                      "' got '" + to_string(rt) + "'");
    }
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
  }

  void visit(CallExpression &node) override {
    // TODO
  }

  void visit(MethodCallExpression &node) override {
    auto recv_type = evaluate(node.receiver.get());
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
    // if param_types.size() == args + 1, first param as receiver and check type
    // match.
    const auto &params = found->param_types;
    const size_t argc = node.arguments.size();
    if (!(params.size() == argc || params.size() == argc + 1)) {
      throw TypeError(
          "method " + node.method_name.name + " signature mismatch: expected " +
          std::to_string(params.size()) + ", got " + std::to_string(argc + 1));
    }
    size_t arg_index_offset = 0;
    if (params.size() == argc + 1) {
      // Validate receiver type matches first param type (basic equality).
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
      // Tuple Type, why handle it?
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
    for (auto &st : node.statements) {
      if (st)
        st->accept(*this);
    }
    SemType t = SemType::primitive(SemPrimitiveKind::UNIT);
    if (node.final_expr)
      t = evaluate(node.final_expr.value().get());
    cache_expr(&node, t);
  }

  void visit(LoopExpression &node) override {
    if (node.body)
      evaluate(node.body.get());
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::UNIT));
  }

  void visit(WhileExpression &node) override {
    if (node.condition) {
      auto ct = evaluate(node.condition.get());
      require_bool(ct, "while condition must be bool");
    }
    if (node.body)
      evaluate(node.body.get());
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::UNIT));
  }

  void visit(ArrayExpression &node) override {
    if (node.repeat) {
      auto el = evaluate(node.repeat->first.get());
      cache_expr(&node, SemType::array(el, 0));
    } else {
      if (node.elements.empty()) {
        cache_expr(&node, SemType::array(
                              SemType::primitive(SemPrimitiveKind::UNIT), 0));
        return;
      }
      auto first = evaluate(node.elements[0].get());
      for (size_t i = 1; i < node.elements.size(); ++i) {
        auto t = evaluate(node.elements[i].get());
        if (!(t == first))
          throw TypeError("array elements have inconsistent types");
      }
      cache_expr(&node, SemType::array(first, node.elements.size()));
    }
  }

  void visit(IndexExpression &node) override {
    auto target_t = evaluate(node.target.get());
    auto idx_type = evaluate(node.index.get());
    require_integer(
        idx_type,
        "array index must be integer"); // TODO: array idx must be positive
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

  void visit(TupleExpression &) override {
    // No Tuple expr.
  }

  void visit(BreakExpression &node) override {
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
  }

  void visit(ContinueExpression &node) override {
    cache_expr(&node, SemType::primitive(SemPrimitiveKind::NEVER));
  }

  void visit(PathExpression &) override {
    // This pass do not visit PathExpr
  }

  void visit(QualifiedPathExpression &) override {
    throw SemanticException("qualified path expression not supported yet");
  }

  void visit(LetStatement &) override {}

  void visit(ExpressionStatement &node) override {
    if (node.expression)
      (void)evaluate(node.expression.get());
  }

  void visit(EmptyStatement &) override {}

  void visit(FunctionDecl &node) override {
    function_return_types.push_back(resolve_type(node.return_type));
    if (node.body)
      evaluate(node.body.value().get());
    function_return_types.pop_back();
  }

  void visit(ConstantItem &) override {}

  void visit(ModuleDecl &) override {}

  void visit(StructDecl &) override {}

  void visit(EnumDecl &) override {}

  void visit(TraitDecl &) override {}

  void visit(ImplDecl &) override {}

  void visit(RootNode &node) override {
    for (auto &c : node.children) {
      if (c)
        c->accept(*this);
    }
  }

  const std::unordered_map<const BaseNode *, SemType> &results() const {
    return cache;
  }

private:
  std::function<SemType(const std::string &)> lookup;
  std::function<SemType(LiteralType)> resolve_type;
  std::vector<SemType> function_return_types; // stack
  std::unordered_map<const BaseNode *, SemType> cache;

  void cache_expr(const BaseNode *n, SemType t) {
    if (cache.find(n) == cache.end())
      cache[n] = t;
    else
      throw SemanticException("revisit the same expr again, why?");
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

  SemType eval_binary(BinaryExpression &bin) {
    auto lt = evaluate(bin.left.get());
    auto rt = evaluate(bin.right.get());
    const auto op = bin.op.type;

    auto is_literal = [](Expression *e) -> bool {
      auto *lit = dynamic_cast<LiteralExpression *>(e);
      if (!lit)
        return false;
      if (!(lit->type.is_base() &&
            lit->type.as_base() == PrimitiveLiteralType::I32))
        return false; // because in sem check we assume all int lit to be i32

      static const std::array<std::string, 4> suffixes = {"i32", "u32", "isize",
                                                          "usize"};
      for (const auto &s : suffixes) {
        if (lit->value.size() >= s.size() &&
            lit->value.compare(lit->value.size() - s.size(), s.size(), s) ==
                0) {
          return false;
        }
      }
      return true;
    };
    
    bool left_literal = is_literal(bin.left.get());
    bool right_literal = is_literal(bin.right.get());

    auto check = [&](bool allow_str = false) -> std::optional<SemType> {
      if (lt.is_primitive() && rt.is_primitive()) {
        auto lk = lt.as_primitive().kind;
        auto rk = rt.as_primitive().kind;
        if (is_integer(lk) && is_integer(rk)) {
          if (lk == rk && (lt == rt))
            return lt;
          if (left_literal && is_integer(rk)) {
            return rt;
          }
          if (right_literal && is_integer(lk)) {
            return lt;
          }
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
      if (lt == rt || (is_integer(lt.as_primitive().kind) &&
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
};

} // namespace rc
