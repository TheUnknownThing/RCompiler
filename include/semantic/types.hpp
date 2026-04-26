#pragma once

#include "ast/types.hpp"
#include <cstdint>
#include <memory>
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
  
#define X(name, display, parse) name,
#include "common/primitive_types.def"
#undef X
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
  static SemType never() { return SemType::primitive(SemPrimitiveKind::NEVER); }
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
  bool is_never() const {
    return is_primitive() && as_primitive().kind == SemPrimitiveKind::NEVER;
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

  static SemType map_primitive(PrimitiveAstType plt) {
    switch (plt) {
#define X(name, display, parse)                                                \
    case PrimitiveAstType::name:                                           \
      return SemType::primitive(SemPrimitiveKind::name);
#include "common/primitive_types.def"
#undef X
    }
    return SemType::primitive(SemPrimitiveKind::UNKNOWN);
  }
};

bool operator==(const SemType &a, const SemType &b);

std::string to_string(SemPrimitiveKind k);

std::string to_string(const SemType &t);

} // namespace rc