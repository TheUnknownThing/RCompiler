#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ast/nodes/base.hpp"
#include "ast/nodes/expr.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "utils/logger.hpp"

namespace rc {
struct ConstValue {
  struct AnyIntValue {
    std::int64_t value;
  }; // why? because we need to distinguish any_int from isize

  using Storage =
      std::variant<std::int32_t,                     // i32
                   std::uint32_t,                    // u32
                   std::int64_t,                     // isize
                   std::uint64_t,                    // usize
                   AnyIntValue,                      // any_int literal
                   std::string,                      // string literals
                   char,                             // char
                   bool,                             // bool
                   std::vector<ConstValue>,          // arrays and tuples
                   std::map<std::string, ConstValue> // struct values
                   >;

  Storage storage;
  SemType type;

  explicit ConstValue(Storage s, SemType t)
      : storage(std::move(s)), type(std::move(t)) {}

  static ConstValue any_int(std::int64_t val) {
    return ConstValue{AnyIntValue{val},
                      SemType::primitive(SemPrimitiveKind::ANY_INT)};
  }

  static ConstValue i32(std::int32_t val) {
    return ConstValue{val, SemType::primitive(SemPrimitiveKind::I32)};
  }

  static ConstValue u32(std::uint32_t val) {
    return ConstValue{val, SemType::primitive(SemPrimitiveKind::U32)};
  }

  static ConstValue isize(std::int64_t val) {
    return ConstValue{val, SemType::primitive(SemPrimitiveKind::ISIZE)};
  }

  static ConstValue usize(std::uint64_t val) {
    return ConstValue{val, SemType::primitive(SemPrimitiveKind::USIZE)};
  }

  static ConstValue string(const std::string &val) {
    return ConstValue{val, SemType::primitive(SemPrimitiveKind::STRING)};
  }

  static ConstValue char_val(char val) {
    return ConstValue{val, SemType::primitive(SemPrimitiveKind::CHAR)};
  }

  static ConstValue bool_val(bool val) {
    return ConstValue{val, SemType::primitive(SemPrimitiveKind::BOOL)};
  }

  static ConstValue array(std::vector<ConstValue> elements, SemType elem_type,
                          std::uint64_t size) {
    return ConstValue{std::move(elements),
                      SemType::array(std::move(elem_type), size)};
  }

  static ConstValue tuple(std::vector<ConstValue> elements) {
    std::vector<SemType> elem_types;
    elem_types.reserve(elements.size());
    for (const auto &elem : elements) {
      elem_types.push_back(elem.type);
    }
    return ConstValue{std::move(elements),
                      SemType::tuple(std::move(elem_types))};
  }

  static ConstValue struct_val(std::map<std::string, ConstValue> fields,
                               const CollectedItem *struct_item) {
    return ConstValue{std::move(fields), SemType::named(struct_item)};
  }

  // Type checkers
  bool is_any_int() const {
    return std::holds_alternative<AnyIntValue>(storage) &&
           type.is_primitive() &&
           type.as_primitive().kind == SemPrimitiveKind::ANY_INT;
  }
  bool is_i32() const {
    return std::holds_alternative<std::int32_t>(storage) &&
           type.is_primitive() &&
           type.as_primitive().kind == SemPrimitiveKind::I32;
  }
  bool is_u32() const {
    return std::holds_alternative<std::uint32_t>(storage) &&
           type.is_primitive() &&
           type.as_primitive().kind == SemPrimitiveKind::U32;
  }
  bool is_isize() const {
    return std::holds_alternative<std::int64_t>(storage) &&
           type.is_primitive() &&
           type.as_primitive().kind == SemPrimitiveKind::ISIZE;
  }
  bool is_usize() const {
    return std::holds_alternative<std::uint64_t>(storage) &&
           type.is_primitive() &&
           type.as_primitive().kind == SemPrimitiveKind::USIZE;
  }
  bool is_string() const {
    return std::holds_alternative<std::string>(storage);
  }
  bool is_char() const { return std::holds_alternative<char>(storage); }
  bool is_bool() const { return std::holds_alternative<bool>(storage); }
  bool is_array() const {
    return std::holds_alternative<std::vector<ConstValue>>(storage) &&
           type.is_array();
  }
  bool is_tuple() const {
    return std::holds_alternative<std::vector<ConstValue>>(storage) &&
           type.is_tuple();
  }
  bool is_struct() const {
    return std::holds_alternative<std::map<std::string, ConstValue>>(storage);
  }

  // Accessors
  std::int64_t as_any_int() const {
    return std::get<AnyIntValue>(storage).value;
  }
  std::int32_t as_i32() const { return std::get<std::int32_t>(storage); }
  std::uint32_t as_u32() const { return std::get<std::uint32_t>(storage); }
  std::int64_t as_isize() const { return std::get<std::int64_t>(storage); }
  std::uint64_t as_usize() const { return std::get<std::uint64_t>(storage); }
  const std::string &as_string() const {
    return std::get<std::string>(storage);
  }
  char as_char() const { return std::get<char>(storage); }
  bool as_bool() const { return std::get<bool>(storage); }
  const std::vector<ConstValue> &as_array() const {
    return std::get<std::vector<ConstValue>>(storage);
  }
  const std::vector<ConstValue> &as_tuple() const {
    return std::get<std::vector<ConstValue>>(storage);
  }
  const std::map<std::string, ConstValue> &as_struct() const {
    return std::get<std::map<std::string, ConstValue>>(storage);
  }
};

class ConstEvaluator {
public:
  explicit ConstEvaluator();

  std::optional<ConstValue> evaluate(const Expression *expr,
                                     ScopeNode *semantic_scope);

private:
  ScopeNode *current_scope = nullptr;

  static bool is_integer_value(const ConstValue &v);
  static ConstValue any_int_to(const ConstValue &v, SemPrimitiveKind target);
  static std::optional<SemPrimitiveKind>
  int_result_kind(const ConstValue &left, const ConstValue &right);
  static std::optional<SemPrimitiveKind>
  as_base_type(const NameExpression &node);

  std::optional<ConstValue> evaluate_literal(const LiteralExpression &node);
  std::optional<ConstValue> evaluate_name(const NameExpression &node);
  std::optional<ConstValue> evaluate_prefix(const PrefixExpression &node);
  std::optional<ConstValue> evaluate_binary(const BinaryExpression &node);
  std::optional<ConstValue> evaluate_array(const ArrayExpression &node);
  std::optional<ConstValue> evaluate_tuple(const TupleExpression &node);
  std::optional<ConstValue> evaluate_index(const IndexExpression &node);
  std::optional<ConstValue>
  evaluate_field_access(const FieldAccessExpression &node);
  std::optional<ConstValue> evaluate_struct(const StructExpression &);

  // Arithmetic operation helpers
  std::optional<ConstValue> evaluate_add(const ConstValue &left,
                                         const ConstValue &right);
  std::optional<ConstValue> evaluate_sub(const ConstValue &left,
                                         const ConstValue &right);
  std::optional<ConstValue> evaluate_mul(const ConstValue &left,
                                         const ConstValue &right);
  std::optional<ConstValue> evaluate_div(const ConstValue &left,
                                         const ConstValue &right);
  std::optional<ConstValue> evaluate_mod(const ConstValue &left,
                                         const ConstValue &right);
  std::optional<ConstValue> evaluate_eq(const ConstValue &left,
                                        const ConstValue &right);
  std::optional<ConstValue> evaluate_ne(const ConstValue &left,
                                        const ConstValue &right);
  std::optional<ConstValue> evaluate_lt(const ConstValue &left,
                                        const ConstValue &right);
  std::optional<ConstValue> evaluate_le(const ConstValue &left,
                                        const ConstValue &right);
  std::optional<ConstValue> evaluate_gt(const ConstValue &left,
                                        const ConstValue &right);
  std::optional<ConstValue> evaluate_ge(const ConstValue &left,
                                        const ConstValue &right);
  std::optional<ConstValue> evaluate_logical_and(const ConstValue &left,
                                                 const ConstValue &right);
  std::optional<ConstValue> evaluate_logical_or(const ConstValue &left,
                                                const ConstValue &right);
  std::optional<ConstValue> evaluate_shl(const ConstValue &left,
                                         const ConstValue &right);
  std::optional<ConstValue> evaluate_shr(const ConstValue &left,
                                         const ConstValue &right);
};

// Implementation

inline ConstEvaluator::ConstEvaluator() {}

inline std::optional<ConstValue>
ConstEvaluator::evaluate(const Expression *expr, ScopeNode *semantic_scope) {
  if (!expr || !semantic_scope) {
    return std::nullopt;
  }

  current_scope = semantic_scope;

  if (auto *literal = dynamic_cast<const LiteralExpression *>(expr)) {
    return evaluate_literal(*literal);
  } else if (auto *name = dynamic_cast<const NameExpression *>(expr)) {
    return evaluate_name(*name);
  } else if (auto *prefix = dynamic_cast<const PrefixExpression *>(expr)) {
    return evaluate_prefix(*prefix);
  } else if (auto *binary = dynamic_cast<const BinaryExpression *>(expr)) {
    return evaluate_binary(*binary);
  } else if (auto *array = dynamic_cast<const ArrayExpression *>(expr)) {
    return evaluate_array(*array);
  } else if (auto *tuple = dynamic_cast<const TupleExpression *>(expr)) {
    return evaluate_tuple(*tuple);
  } else if (auto *index = dynamic_cast<const IndexExpression *>(expr)) {
    return evaluate_index(*index);
  } else if (auto *field = dynamic_cast<const FieldAccessExpression *>(expr)) {
    return evaluate_field_access(*field);
  } else if (auto *struct_expr = dynamic_cast<const StructExpression *>(expr)) {
    return evaluate_struct(*struct_expr);
  } else if (auto *group = dynamic_cast<const GroupExpression *>(expr)) {
    return evaluate(group->inner.get(), semantic_scope);
  }

  return std::nullopt;
}

inline bool ConstEvaluator::is_integer_value(const ConstValue &v) {
  return v.is_any_int() || v.is_i32() || v.is_u32() || v.is_isize() ||
         v.is_usize();
}

inline ConstValue ConstEvaluator::any_int_to(const ConstValue &v,
                                             SemPrimitiveKind target) {
  if (!v.is_any_int())
    return v;
  switch (target) {
  case SemPrimitiveKind::I32:
    if (v.as_any_int() < std::numeric_limits<std::int32_t>::min() ||
        v.as_any_int() > std::numeric_limits<std::int32_t>::max()) {
      throw SemanticException("integer literal out of range for i32");
    }
    return ConstValue::i32(static_cast<std::int32_t>(v.as_any_int()));
  case SemPrimitiveKind::U32:
    if (v.as_any_int() < 0 ||
        v.as_any_int() > std::numeric_limits<std::uint32_t>::max()) {
      throw SemanticException("integer literal out of range for u32");
    }
    return ConstValue::u32(static_cast<std::uint32_t>(v.as_any_int()));
  case SemPrimitiveKind::ISIZE:
    if (v.as_any_int() < std::numeric_limits<std::int64_t>::min() ||
        v.as_any_int() > std::numeric_limits<std::int64_t>::max()) {
      throw SemanticException("integer literal out of range for isize");
    }
    return ConstValue::isize(static_cast<std::int64_t>(v.as_any_int()));
  case SemPrimitiveKind::USIZE:
    if (v.as_any_int() < 0 || static_cast<std::uint64_t>(v.as_any_int()) >
                                  std::numeric_limits<std::uint64_t>::max()) {
      throw SemanticException("integer literal out of range for usize");
    }
    return ConstValue::usize(static_cast<std::uint64_t>(v.as_any_int()));
  default:
    break;
  }
  return v;
}

inline std::optional<SemPrimitiveKind>
ConstEvaluator::int_result_kind(const ConstValue &left,
                                const ConstValue &right) {
  if (!is_integer_value(left) || !is_integer_value(right))
    return std::nullopt;

  auto lk = left.type.as_primitive().kind;
  auto rk = right.type.as_primitive().kind;

  if (lk == rk)
    return lk;

  if (lk == SemPrimitiveKind::ANY_INT && rk != SemPrimitiveKind::ANY_INT)
    return rk;
  if (rk == SemPrimitiveKind::ANY_INT && lk != SemPrimitiveKind::ANY_INT)
    return lk;

  if (lk == SemPrimitiveKind::ANY_INT && rk == SemPrimitiveKind::ANY_INT)
    return SemPrimitiveKind::ANY_INT;

  return std::nullopt;
}

inline std::optional<SemPrimitiveKind>
ConstEvaluator::as_base_type(const NameExpression &node) {
  const std::string &name = node.name;
  if (name == "i32")
    return SemPrimitiveKind::I32;
  if (name == "u32")
    return SemPrimitiveKind::U32;
  if (name == "isize")
    return SemPrimitiveKind::ISIZE;
  if (name == "usize")
    return SemPrimitiveKind::USIZE;

  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_literal(const LiteralExpression &node) {
  try {
    if (node.type.is_base()) {
      switch (node.type.as_base()) {
      case PrimitiveLiteralType::ANY_INT: {
        std::int64_t val = std::stoll(node.value);
        return ConstValue::any_int(val);
      }
      case PrimitiveLiteralType::I32: {
        std::int32_t val = std::stoi(node.value);
        return ConstValue::i32(val);
      }
      case PrimitiveLiteralType::U32: {
        std::uint32_t val = std::stoul(node.value);
        return ConstValue::u32(val);
      }
      case PrimitiveLiteralType::ISIZE: {
        std::int64_t val = std::stoll(node.value);
        return ConstValue::isize(val);
      }
      case PrimitiveLiteralType::USIZE: {
        std::uint64_t val = std::stoull(node.value);
        return ConstValue::usize(val);
      }
      case PrimitiveLiteralType::STRING: {
        return ConstValue::string(node.value);
      }
      case PrimitiveLiteralType::CHAR: {
        if (node.value.length() >= 3) {
          char val = node.value[1]; // skip ''
          return ConstValue::char_val(val);
        }
        break;
      }
      case PrimitiveLiteralType::BOOL: {
        bool val = (node.value == "true");
        return ConstValue::bool_val(val);
      }
      default:
        break;
      }
    }
  } catch (const std::exception &e) {
    LOG_ERROR("[ConstEvaluator] Failed to parse literal: " + node.value +
              " - " + e.what());
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_name(const NameExpression &node) {
  if (current_scope) {
    for (auto *scope = current_scope; scope; scope = scope->parent) {
      if (auto *item = scope->find_value_item(node.name)) {
        if (item->kind == ItemKind::Constant && item->has_constant_meta()) {
          const auto &meta = item->as_constant_meta();
          LOG_DEBUG("[ConstEvaluator] Found constant reference: " + node.name);
          // this constant has already been evaluated, use its value
          if (meta.evaluated_value) {
            return *meta.evaluated_value;
          }
          // it has an initializer, evaluate it recursively
          if (meta.decl && meta.decl->value) {
            auto evaluated = evaluate(meta.decl->value->get(), current_scope);
            if (evaluated) {
              const_cast<ConstantMetaData &>(meta).evaluated_value =
                  std::make_shared<ConstValue>(std::move(*evaluated));
              return *meta.evaluated_value;
            }
          }
          throw SemanticException("could not evaluate constant: " + node.name);
        }
      }
    }
    throw SemanticException("name not found or not constant: " + node.name);
  }
  throw SemanticException("no current scope to resolve name: " + node.name);
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_prefix(const PrefixExpression &node) {
  if (!node.right)
    return std::nullopt;

  auto operand = evaluate(node.right.get(), current_scope);
  if (!operand)
    return std::nullopt;

  switch (node.op.type) {
  case TokenType::MINUS: {
    if (operand->is_i32()) {
      return ConstValue::i32(-operand->as_i32());
    }
    if (operand->is_isize()) {
      return ConstValue::isize(-operand->as_isize());
    }
    if (operand->is_any_int()) {
      return ConstValue::any_int(-operand->as_any_int());
    }
    break;
  }
  case TokenType::NOT: {
    if (operand->is_bool()) {
      return ConstValue::bool_val(!operand->as_bool());
    }
    break;
  }
  default:
    break;
  }

  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_binary(const BinaryExpression &node) {
  if (!node.left || !node.right)
    return std::nullopt;

  // Handle as cast
  if (node.op.type == TokenType::AS) {
    auto left = evaluate(node.left.get(), current_scope);
    if (!left)
      return std::nullopt;

    if (!is_integer_value(*left))
      return std::nullopt;

    if (!dynamic_cast<NameExpression *>(node.right.get()))
      return std::nullopt;

    auto target_base =
        as_base_type(*dynamic_cast<NameExpression *>(node.right.get()));
    if (!target_base)
      return std::nullopt;
    SemPrimitiveKind target_kind = *target_base;
    return any_int_to(*left, target_kind);
  }

  auto left = evaluate(node.left.get(), current_scope);
  auto right = evaluate(node.right.get(), current_scope);

  if (!left || !right)
    return std::nullopt;

  // Arithmetic operations
  switch (node.op.type) {
  case TokenType::PLUS:
    return evaluate_add(*left, *right);
  case TokenType::MINUS:
    return evaluate_sub(*left, *right);
  case TokenType::STAR:
    return evaluate_mul(*left, *right);
  case TokenType::SLASH:
    return evaluate_div(*left, *right);
  case TokenType::PERCENT:
    return evaluate_mod(*left, *right);
  case TokenType::EQ:
    return evaluate_eq(*left, *right);
  case TokenType::NE:
    return evaluate_ne(*left, *right);
  case TokenType::LT:
    return evaluate_lt(*left, *right);
  case TokenType::LE:
    return evaluate_le(*left, *right);
  case TokenType::GT:
    return evaluate_gt(*left, *right);
  case TokenType::GE:
    return evaluate_ge(*left, *right);
  case TokenType::AND:
    return evaluate_logical_and(*left, *right);
  case TokenType::OR:
    return evaluate_logical_or(*left, *right);
  case TokenType::SHL:
    return evaluate_shl(*left, *right);
  case TokenType::SHR:
    return evaluate_shr(*left, *right);
  default:
    break;
  }

  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_array(const ArrayExpression &node) {
  if (node.repeat) {
    // [expr; size] form
    auto value = evaluate(node.repeat->first.get(), current_scope);
    auto size_expr = evaluate(node.repeat->second.get(), current_scope);

    if (!value || !size_expr) {
      return std::nullopt;
    }

    std::uint64_t size = 0;
    if (size_expr->is_usize()) {
      size = size_expr->as_usize();
    } else if (size_expr->is_any_int()) {
      size = static_cast<std::uint64_t>(size_expr->as_any_int());
    } else {
      return std::nullopt;
    }

    std::vector<ConstValue> elements;
    elements.reserve(size);

    for (std::uint64_t i = 0; i < size; ++i) {
      elements.push_back(*value);
    }

    return ConstValue::array(std::move(elements), value->type, size);
  } else {
    // [e1, e2, ...] form
    std::vector<ConstValue> elements;
    elements.reserve(node.elements.size());

    SemType elem_type = SemType::primitive(SemPrimitiveKind::UNKNOWN);
    bool first = true;

    for (const auto &elem_expr : node.elements) {
      auto elem_val = evaluate(elem_expr.get(), current_scope);
      if (!elem_val) {
        return std::nullopt;
      }

      if (first) {
        elem_type = elem_val->type;
        first = false;
      }

      elements.push_back(std::move(*elem_val));
    }

    const auto size = static_cast<std::uint64_t>(elements.size());
    return ConstValue::array(std::move(elements), std::move(elem_type), size);
  }
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_tuple(const TupleExpression &node) {
  std::vector<ConstValue> elements;
  elements.reserve(node.elements.size());

  for (const auto &elem_expr : node.elements) {
    auto elem_val = evaluate(elem_expr.get(), current_scope);
    if (!elem_val) {
      return std::nullopt;
    }
    elements.push_back(std::move(*elem_val));
  }

  return ConstValue::tuple(std::move(elements));
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_index(const IndexExpression &node) {
  if (!node.target || !node.index)
    return std::nullopt;

  auto target = evaluate(node.target.get(), current_scope);
  auto index = evaluate(node.index.get(), current_scope);

  if (!target || !index) {
    return std::nullopt;
  }

  std::uint64_t idx = 0;
  if (index->is_usize()) {
    idx = index->as_usize();
  } else if (index->is_any_int()) {
    idx = static_cast<std::uint64_t>(index->as_any_int());
  } else {
    return std::nullopt;
  }

  if (target->is_array()) {
    const auto &elements = target->as_array();
    if (idx < elements.size()) {
      return elements[idx];
    }
    throw SemanticException("array index out of bounds");
  }

  if (target->is_tuple()) {
    const auto &elements = target->as_tuple();
    if (idx < elements.size()) {
      return elements[idx];
    }
    throw SemanticException("tuple index out of bounds");
  }

  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_field_access(const FieldAccessExpression &node) {
  if (!node.target)
    return std::nullopt;

  auto target = evaluate(node.target.get(), current_scope);
  if (!target)
    return std::nullopt;

  if (target->is_struct()) {
    const auto &fields = target->as_struct();
    auto it = fields.find(node.field_name);
    if (it != fields.end()) {
      return it->second;
    }
    throw SemanticException("field '" + node.field_name +
                            "' not found in struct");
  }

  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_struct(const StructExpression &) {
  // TODO: Evaluate struct expressions
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_add(const ConstValue &left, const ConstValue &right) {
  auto kind = int_result_kind(left, right);
  if (!kind)
    return std::nullopt;

  if (*kind == SemPrimitiveKind::ANY_INT) {
    return ConstValue::any_int(left.as_any_int() + right.as_any_int());
  }

  auto l = any_int_to(left, *kind);
  auto r = any_int_to(right, *kind);

  switch (*kind) {
  case SemPrimitiveKind::I32:
    return ConstValue::i32(l.as_i32() + r.as_i32());
  case SemPrimitiveKind::U32:
    return ConstValue::u32(l.as_u32() + r.as_u32());
  case SemPrimitiveKind::ISIZE:
    return ConstValue::isize(l.as_isize() + r.as_isize());
  case SemPrimitiveKind::USIZE:
    return ConstValue::usize(l.as_usize() + r.as_usize());
  default:
    break;
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_sub(const ConstValue &left, const ConstValue &right) {
  auto kind = int_result_kind(left, right);
  if (!kind)
    return std::nullopt;

  if (*kind == SemPrimitiveKind::ANY_INT) {
    return ConstValue::any_int(left.as_any_int() - right.as_any_int());
  }

  auto l = any_int_to(left, *kind);
  auto r = any_int_to(right, *kind);
  switch (*kind) {
  case SemPrimitiveKind::I32:
    return ConstValue::i32(l.as_i32() - r.as_i32());
  case SemPrimitiveKind::U32:
    return ConstValue::u32(l.as_u32() - r.as_u32());
  case SemPrimitiveKind::ISIZE:
    return ConstValue::isize(l.as_isize() - r.as_isize());
  case SemPrimitiveKind::USIZE:
    return ConstValue::usize(l.as_usize() - r.as_usize());
  default:
    break;
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_mul(const ConstValue &left, const ConstValue &right) {
  auto kind = int_result_kind(left, right);
  if (!kind)
    return std::nullopt;

  if (*kind == SemPrimitiveKind::ANY_INT) {
    return ConstValue::any_int(left.as_any_int() * right.as_any_int());
  }

  auto l = any_int_to(left, *kind);
  auto r = any_int_to(right, *kind);
  switch (*kind) {
  case SemPrimitiveKind::I32:
    return ConstValue::i32(l.as_i32() * r.as_i32());
  case SemPrimitiveKind::U32:
    return ConstValue::u32(l.as_u32() * r.as_u32());
  case SemPrimitiveKind::ISIZE:
    return ConstValue::isize(l.as_isize() * r.as_isize());
  case SemPrimitiveKind::USIZE:
    return ConstValue::usize(l.as_usize() * r.as_usize());
  default:
    break;
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_div(const ConstValue &left, const ConstValue &right) {
  auto kind = int_result_kind(left, right);
  if (!kind)
    return std::nullopt;

  if (*kind == SemPrimitiveKind::ANY_INT) {
    if (right.as_any_int() == 0) {
      throw SemanticException("division by zero");
    }
    return ConstValue::any_int(left.as_any_int() / right.as_any_int());
  }

  auto l = any_int_to(left, *kind);
  auto r = any_int_to(right, *kind);
  switch (*kind) {
  case SemPrimitiveKind::I32:
    if (r.as_i32() == 0) {
      throw SemanticException("division by zero");
    }
    return ConstValue::i32(l.as_i32() / r.as_i32());
  case SemPrimitiveKind::U32:
    if (r.as_u32() == 0) {
      throw SemanticException("division by zero");
    }
    return ConstValue::u32(l.as_u32() / r.as_u32());
  case SemPrimitiveKind::ISIZE:
    if (r.as_isize() == 0) {
      throw SemanticException("division by zero");
    }
    return ConstValue::isize(l.as_isize() / r.as_isize());
  case SemPrimitiveKind::USIZE:
    if (r.as_usize() == 0) {
      throw SemanticException("division by zero");
    }
    return ConstValue::usize(l.as_usize() / r.as_usize());
  default:
    break;
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_mod(const ConstValue &left, const ConstValue &right) {
  auto kind = int_result_kind(left, right);
  if (!kind)
    return std::nullopt;

  if (*kind == SemPrimitiveKind::ANY_INT) {
    if (right.as_any_int() == 0) {
      throw SemanticException("modulo by zero");
    }
    return ConstValue::any_int(left.as_any_int() % right.as_any_int());
  }

  auto l = any_int_to(left, *kind);
  auto r = any_int_to(right, *kind);
  switch (*kind) {
  case SemPrimitiveKind::I32:
    if (r.as_i32() == 0) {
      throw SemanticException("modulo by zero");
    }
    return ConstValue::i32(l.as_i32() % r.as_i32());
  case SemPrimitiveKind::U32:
    if (r.as_u32() == 0) {
      throw SemanticException("modulo by zero");
    }
    return ConstValue::u32(l.as_u32() % r.as_u32());
  case SemPrimitiveKind::ISIZE:
    if (r.as_isize() == 0) {
      throw SemanticException("modulo by zero");
    }
    return ConstValue::isize(l.as_isize() % r.as_isize());
  case SemPrimitiveKind::USIZE:
    if (r.as_usize() == 0) {
      throw SemanticException("modulo by zero");
    }
    return ConstValue::usize(l.as_usize() % r.as_usize());
  default:
    break;
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_eq(const ConstValue &left, const ConstValue &right) {
  if (left.is_i32() && right.is_i32()) {
    return ConstValue::bool_val(left.as_i32() == right.as_i32());
  }
  if (left.is_u32() && right.is_u32()) {
    return ConstValue::bool_val(left.as_u32() == right.as_u32());
  }
  if (left.is_isize() && right.is_isize()) {
    return ConstValue::bool_val(left.as_isize() == right.as_isize());
  }
  if (left.is_usize() && right.is_usize()) {
    return ConstValue::bool_val(left.as_usize() == right.as_usize());
  }
  if (left.is_bool() && right.is_bool()) {
    return ConstValue::bool_val(left.as_bool() == right.as_bool());
  }
  if (left.is_char() && right.is_char()) {
    return ConstValue::bool_val(left.as_char() == right.as_char());
  }
  if (is_integer_value(left) && is_integer_value(right)) {
    auto kind = int_result_kind(left, right);
    if (!kind)
      return std::nullopt;
    if (*kind == SemPrimitiveKind::ANY_INT) {
      return ConstValue::bool_val(left.as_any_int() == right.as_any_int());
    }
    auto l = any_int_to(left, *kind);
    auto r = any_int_to(right, *kind);
    switch (*kind) {
    case SemPrimitiveKind::I32:
      return ConstValue::bool_val(l.as_i32() == r.as_i32());
    case SemPrimitiveKind::U32:
      return ConstValue::bool_val(l.as_u32() == r.as_u32());
    case SemPrimitiveKind::ISIZE:
      return ConstValue::bool_val(l.as_isize() == r.as_isize());
    case SemPrimitiveKind::USIZE:
      return ConstValue::bool_val(l.as_usize() == r.as_usize());
    default:
      break;
    }
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_ne(const ConstValue &left, const ConstValue &right) {
  auto eq_result = evaluate_eq(left, right);
  if (eq_result && eq_result->is_bool()) {
    return ConstValue::bool_val(!eq_result->as_bool());
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_lt(const ConstValue &left, const ConstValue &right) {
  if (left.is_i32() && right.is_i32()) {
    return ConstValue::bool_val(left.as_i32() < right.as_i32());
  }
  if (left.is_u32() && right.is_u32()) {
    return ConstValue::bool_val(left.as_u32() < right.as_u32());
  }
  if (left.is_isize() && right.is_isize()) {
    return ConstValue::bool_val(left.as_isize() < right.as_isize());
  }
  if (left.is_usize() && right.is_usize()) {
    return ConstValue::bool_val(left.as_usize() < right.as_usize());
  }
  if (left.is_char() && right.is_char()) {
    return ConstValue::bool_val(left.as_char() < right.as_char());
  }
  if (is_integer_value(left) && is_integer_value(right)) {
    auto kind = int_result_kind(left, right);
    if (!kind)
      return std::nullopt;
    if (*kind == SemPrimitiveKind::ANY_INT) {
      return ConstValue::bool_val(left.as_any_int() < right.as_any_int());
    }
    auto l = any_int_to(left, *kind);
    auto r = any_int_to(right, *kind);
    switch (*kind) {
    case SemPrimitiveKind::I32:
      return ConstValue::bool_val(l.as_i32() < r.as_i32());
    case SemPrimitiveKind::U32:
      return ConstValue::bool_val(l.as_u32() < r.as_u32());
    case SemPrimitiveKind::ISIZE:
      return ConstValue::bool_val(l.as_isize() < r.as_isize());
    case SemPrimitiveKind::USIZE:
      return ConstValue::bool_val(l.as_usize() < r.as_usize());
    default:
      break;
    }
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_le(const ConstValue &left, const ConstValue &right) {
  auto lt_result = evaluate_lt(left, right);
  auto eq_result = evaluate_eq(left, right);
  if (lt_result && eq_result && lt_result->is_bool() && eq_result->is_bool()) {
    return ConstValue::bool_val(lt_result->as_bool() || eq_result->as_bool());
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_gt(const ConstValue &left, const ConstValue &right) {
  auto le_result = evaluate_le(left, right);
  if (le_result && le_result->is_bool()) {
    return ConstValue::bool_val(!le_result->as_bool());
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_ge(const ConstValue &left, const ConstValue &right) {
  auto lt_result = evaluate_lt(left, right);
  if (lt_result && lt_result->is_bool()) {
    return ConstValue::bool_val(!lt_result->as_bool());
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_logical_and(const ConstValue &left,
                                     const ConstValue &right) {
  if (left.is_bool() && right.is_bool()) {
    return ConstValue::bool_val(left.as_bool() && right.as_bool());
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_logical_or(const ConstValue &left,
                                    const ConstValue &right) {
  if (left.is_bool() && right.is_bool()) {
    return ConstValue::bool_val(left.as_bool() || right.as_bool());
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_shl(const ConstValue &left, const ConstValue &right) {
  auto kind = int_result_kind(left, right);
  if (!kind)
    return std::nullopt;

  if (*kind == SemPrimitiveKind::ANY_INT) {
    return ConstValue::any_int(left.as_any_int() << right.as_any_int());
  }

  auto l = any_int_to(left, *kind);
  auto r = any_int_to(right, *kind);
  switch (*kind) {
  case SemPrimitiveKind::I32:
    return ConstValue::i32(l.as_i32() << r.as_i32());
  case SemPrimitiveKind::U32:
    return ConstValue::u32(l.as_u32() << r.as_u32());
  case SemPrimitiveKind::ISIZE:
    return ConstValue::isize(l.as_isize() << r.as_isize());
  case SemPrimitiveKind::USIZE:
    return ConstValue::usize(l.as_usize() << r.as_usize());
  default:
    break;
  }
  return std::nullopt;
}

inline std::optional<ConstValue>
ConstEvaluator::evaluate_shr(const ConstValue &left, const ConstValue &right) {
  auto kind = int_result_kind(left, right);
  if (!kind)
    return std::nullopt;

  if (*kind == SemPrimitiveKind::ANY_INT) {
    return ConstValue::any_int(left.as_any_int() >> right.as_any_int());
  }

  auto l = any_int_to(left, *kind);
  auto r = any_int_to(right, *kind);
  switch (*kind) {
  case SemPrimitiveKind::I32:
    return ConstValue::i32(l.as_i32() >> r.as_i32());
  case SemPrimitiveKind::U32:
    return ConstValue::u32(l.as_u32() >> r.as_u32());
  case SemPrimitiveKind::ISIZE:
    return ConstValue::isize(l.as_isize() >> r.as_isize());
  case SemPrimitiveKind::USIZE:
    return ConstValue::usize(l.as_usize() >> r.as_usize());
  default:
    break;
  }
  return std::nullopt;
}

} // namespace rc