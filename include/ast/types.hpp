#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace rc {

enum class PrimitiveAstType {
#define X(name, display, parse) name,
#include "common/primitive_types.def"
#undef X
};

class Expression;

struct AstType {
  struct Tuple {
    std::vector<AstType> elements;
  };
  struct Array {
    std::shared_ptr<AstType> element;
    std::shared_ptr<Expression> size;

    int64_t actual_size = -1; // this size can only be set in const eval
  };
  struct Slice {
    std::shared_ptr<AstType> element;
  };
  struct Path {
    std::vector<std::string> segments;
  };
  struct Union {
    std::vector<AstType> alternatives;
  };
  struct Reference {
    std::shared_ptr<AstType> target;
    bool is_mutable;
  };

  using Storage = std::variant<PrimitiveAstType, Tuple, Array, Slice, Path,
                               Union, Reference>;

  Storage storage;

  AstType() = default;
  explicit AstType(Storage s) : storage(std::move(s)) {}
  AstType(PrimitiveAstType b) : storage(b) {}

  static AstType base(PrimitiveAstType b) { return AstType{b}; }
  static AstType tuple(std::vector<AstType> elems) {
    return AstType{Tuple{std::move(elems)}};
  }
  static AstType array(AstType elem,
                           std::shared_ptr<Expression> size_expr) {
    return AstType{Array{std::make_shared<AstType>(std::move(elem)),
                             std::move(size_expr)}};
  }
  static AstType slice(AstType elem) {
    return AstType{Slice{std::make_shared<AstType>(std::move(elem))}};
  }
  static AstType path(std::vector<std::string> segments) {
    return AstType{Path{std::move(segments)}};
  }
  static AstType union_of(std::vector<AstType> alts) {
    return AstType{Union{std::move(alts)}};
  }
  static AstType reference(AstType target, bool is_mutable) {
    return AstType{Reference{
        std::make_shared<AstType>(std::move(target)), is_mutable}};
  }

  bool is_base() const {
    return std::holds_alternative<PrimitiveAstType>(storage);
  }
  bool is_tuple() const { return std::holds_alternative<Tuple>(storage); }
  bool is_array() const { return std::holds_alternative<Array>(storage); }
  bool is_slice() const { return std::holds_alternative<Slice>(storage); }
  bool is_path() const { return std::holds_alternative<Path>(storage); }
  bool is_reference() const {
    return std::holds_alternative<Reference>(storage);
  }

  PrimitiveAstType as_base() const {
    return std::get<PrimitiveAstType>(storage);
  }
  const std::vector<AstType> &as_tuple() const {
    return std::get<Tuple>(storage).elements;
  }
  std::vector<AstType> &as_tuple() {
    return std::get<Tuple>(storage).elements;
  }
  const Array &as_array() const { return std::get<Array>(storage); }
  Array &as_array() { return std::get<Array>(storage); }
  const Slice &as_slice() const { return std::get<Slice>(storage); }
  Slice &as_slice() { return std::get<Slice>(storage); }
  const Path &as_path() const { return std::get<Path>(storage); }
  Path &as_path() { return std::get<Path>(storage); }
  const std::vector<AstType> &as_union() const {
    return std::get<Union>(storage).alternatives;
  }
  std::vector<AstType> &as_union() {
    return std::get<Union>(storage).alternatives;
  }
  const Reference &as_reference() const { return std::get<Reference>(storage); }
  Reference &as_reference() { return std::get<Reference>(storage); }
};

bool operator==(const AstType &a, const AstType &b);

extern const std::map<PrimitiveAstType, std::string>
  literal_type_reverse_map;

extern const std::map<std::string, AstType> literal_type_map;

std::string to_string(const AstType &t);

} // namespace rc