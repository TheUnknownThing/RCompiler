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

  void visit(MethodCallExpression &) override {
    // TODO
  }

  void visit(FieldAccessExpression &) override {
    // TODO
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
    switch (op) {
    case TokenType::PLUS: {
      if (lt.is_primitive() && rt.is_primitive()) {
        auto lk = lt.as_primitive().kind;
        auto rk = rt.as_primitive().kind;
        if (is_integer(lk) && lk == rk && lt == rt)
          return lt;
        if (is_str(lk) && lk == rk)
          return lt;
      }
      break;
    }
    case TokenType::MINUS:
    case TokenType::STAR:
    case TokenType::SLASH:
    case TokenType::PERCENT: {
      if (lt.is_primitive() && rt.is_primitive() && lt == rt &&
          is_integer(lt.as_primitive().kind))
        return lt;
      break;
    }
    case TokenType::AMPERSAND:
    case TokenType::PIPE:
    case TokenType::CARET: {
      if (lt.is_primitive() && rt.is_primitive() && lt == rt &&
          is_integer(lt.as_primitive().kind))
        return lt;
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
      break;
    }
    case TokenType::LT:
    case TokenType::LE:
    case TokenType::GT:
    case TokenType::GE: {
      if (lt.is_primitive() && rt.is_primitive() && lt == rt &&
          is_integer(lt.as_primitive().kind))
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
};

} // namespace rc
