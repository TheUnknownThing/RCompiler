#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rc {
struct CollectedItem; // fwd decl
}

namespace rc {

struct SemType;
struct SemPrimitiveType;
struct SemTupleType;
struct SemArrayType;
struct SemSliceType;
struct SemReferenceType; // reference type &T or &mut T
struct SemNamedItemType; // struct/enum/trait reference resolved to NameItem
struct SemUnknownType;

enum class SemPrimitiveKind {
  ANY_INT,
  I32,
  U32,
  ISIZE,
  USIZE,
  STRING,
  RAW_STRING,
  C_STRING,
  RAW_C_STRING,
  CHAR,
  BOOL,
  NEVER,
  UNIT,
  UNKNOWN
};

struct SemPrimitiveType {
  SemPrimitiveKind kind;
};

struct SemTupleType {
  std::vector<SemType> elements;
};

struct SemArrayType {
  std::shared_ptr<SemType> element;
  std::uint64_t size;
};

struct SemSliceType {
  std::shared_ptr<SemType> element;
};

struct SemReferenceType {
  std::shared_ptr<SemType> target;
  bool is_mutable;
};

struct SemNamedItemType {
  const CollectedItem *item;
};

struct SemUnknownType {
  unsigned id;
};

struct SemType {
  using Storage =
      std::variant<SemPrimitiveType, SemTupleType, SemArrayType, SemSliceType,
                   SemReferenceType, SemNamedItemType, SemUnknownType>;
  Storage storage;

  SemType() : storage(SemPrimitiveType{SemPrimitiveKind::UNKNOWN}) {}
  explicit SemType(Storage s) : storage(std::move(s)) {}

  static SemType primitive(SemPrimitiveKind k) {
    return SemType{SemPrimitiveType{k}};
  }
  static SemType tuple(std::vector<SemType> elems) {
    return SemType{SemTupleType{std::move(elems)}};
  }
  static SemType array(SemType elem, std::uint64_t size) {
    return SemType{
        SemArrayType{std::make_shared<SemType>(std::move(elem)), size}};
  }
  static SemType slice(SemType elem) {
    return SemType{SemSliceType{std::make_shared<SemType>(std::move(elem))}};
  }
  static SemType reference(SemType target, bool is_mutable) {
    return SemType{SemReferenceType{
        std::make_shared<SemType>(std::move(target)), is_mutable}};
  }
  static SemType named(const CollectedItem *ci) {
    return SemType{SemNamedItemType{ci}};
  }
  static SemType unknown(unsigned id) { return SemType{SemUnknownType{id}}; }

  bool is_primitive() const {
    return std::holds_alternative<SemPrimitiveType>(storage);
  }
  bool is_tuple() const {
    return std::holds_alternative<SemTupleType>(storage);
  }
  bool is_array() const {
    return std::holds_alternative<SemArrayType>(storage);
  }
  bool is_slice() const {
    return std::holds_alternative<SemSliceType>(storage);
  }
  bool is_reference() const {
    return std::holds_alternative<SemReferenceType>(storage);
  }
  bool is_named() const {
    return std::holds_alternative<SemNamedItemType>(storage);
  }
  bool is_unknown() const {
    return std::holds_alternative<SemUnknownType>(storage);
  }

  const SemPrimitiveType &as_primitive() const {
    return std::get<SemPrimitiveType>(storage);
  }
  const SemTupleType &as_tuple() const {
    return std::get<SemTupleType>(storage);
  }
  const SemArrayType &as_array() const {
    return std::get<SemArrayType>(storage);
  }
  const SemSliceType &as_slice() const {
    return std::get<SemSliceType>(storage);
  }
  const SemReferenceType &as_reference() const {
    return std::get<SemReferenceType>(storage);
  }
  const SemNamedItemType &as_named() const {
    return std::get<SemNamedItemType>(storage);
  }
  const SemUnknownType &as_unknown() const {
    return std::get<SemUnknownType>(storage);
  }
};

inline bool operator==(const SemType &a, const SemType &b) {
  if (a.storage.index() != b.storage.index())
    return false;
  if (a.is_primitive())
    return a.as_primitive().kind == b.as_primitive().kind;
  if (a.is_tuple())
    return a.as_tuple().elements == b.as_tuple().elements;
  if (a.is_array())
    return *a.as_array().element == *b.as_array().element &&
           a.as_array().size == b.as_array().size;
  if (a.is_slice())
    return *a.as_slice().element == *b.as_slice().element;
  if (a.is_reference())
    return *a.as_reference().target == *b.as_reference().target &&
           a.as_reference().is_mutable == b.as_reference().is_mutable;
  if (a.is_named())
    return a.as_named().item == b.as_named().item;
  if (a.is_unknown())
    return a.as_unknown().id == b.as_unknown().id;
  return false;
}

inline std::string to_string(SemPrimitiveKind k) {
  switch (k) {
  case SemPrimitiveKind::ANY_INT:
    return "any_int";
  case SemPrimitiveKind::I32:
    return "i32";
  case SemPrimitiveKind::U32:
    return "u32";
  case SemPrimitiveKind::ISIZE:
    return "isize";
  case SemPrimitiveKind::USIZE:
    return "usize";
  case SemPrimitiveKind::STRING:
    return "string";
  case SemPrimitiveKind::RAW_STRING:
    return "raw_string";
  case SemPrimitiveKind::C_STRING:
    return "c_string";
  case SemPrimitiveKind::RAW_C_STRING:
    return "raw_c_string";
  case SemPrimitiveKind::CHAR:
    return "char";
  case SemPrimitiveKind::BOOL:
    return "bool";
  case SemPrimitiveKind::NEVER:
    return "!";
  case SemPrimitiveKind::UNIT:
    return "()";
  case SemPrimitiveKind::UNKNOWN:
    return "<unknown>";
  }
  return "<invalid>";
}

inline std::string to_string(const SemType &t) {
  if (t.is_primitive())
    return to_string(t.as_primitive().kind);
  if (t.is_tuple()) {
    const auto &els = t.as_tuple().elements;
    std::string out = "(";
    for (size_t i = 0; i < els.size(); ++i) {
      if (i)
        out += ", ";
      out += to_string(els[i]);
    }
    out += ")";
    return out;
  }
  if (t.is_array()) {
    return "[" + to_string(*t.as_array().element) + "; " +
           std::to_string(t.as_array().size) + "]";
  }
  if (t.is_slice()) {
    return "[" + to_string(*t.as_slice().element) + "]";
  }
  if (t.is_reference()) {
    const auto &ref = t.as_reference();
    std::string result = "&";
    if (ref.is_mutable) {
      result += "mut ";
    }
    result += to_string(*ref.target);
    return result;
  }
  if (t.is_named()) {
    return "<named-item>";
  }
  if (t.is_unknown()) {
    return "_" + std::to_string(t.as_unknown().id);
  }
  return "<type>";
}

} // namespace rc