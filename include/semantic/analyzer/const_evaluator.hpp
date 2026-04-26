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

} // namespace rc