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

inline bool operator==(const AstType &a, const AstType &b) {
  if (a.storage.index() != b.storage.index())
    return false;
  if (a.is_base())
    return a.as_base() == b.as_base();
  if (a.is_tuple())
    return a.as_tuple() == b.as_tuple();
  if (a.is_array()) {
    const auto &ae = a.as_array();
    const auto &be = b.as_array();
    return *ae.element == *be.element && ae.size == be.size;
  }
  if (a.is_slice()) {
    return *a.as_slice().element == *b.as_slice().element;
  }
  if (a.is_path()) {
    return a.as_path().segments == b.as_path().segments;
  }
  if (a.is_reference() && b.is_reference()) {
    return *a.as_reference().target == *b.as_reference().target &&
           a.as_reference().is_mutable == b.as_reference().is_mutable;
  }
  return false;
}

inline const std::map<PrimitiveAstType, std::string>
  literal_type_reverse_map = {
#define X(name, display, parse) {PrimitiveAstType::name, display},
#include "common/primitive_types.def"
#undef X
};

inline const std::map<std::string, AstType> literal_type_map = {
#define X(name, display, parse) {parse, AstType::base(PrimitiveAstType::name)},
#include "common/primitive_types.def"
#undef X
};

inline std::string to_string(const AstType &t) {
  if (t.is_base()) {
    auto it = literal_type_reverse_map.find(t.as_base());
    return it != literal_type_reverse_map.end() ? it->second : "<unknown>";
  }
  if (t.is_tuple()) {
    const auto &elems = t.as_tuple();
    std::string out = "(";
    for (size_t i = 0; i < elems.size(); ++i) {
      if (i)
        out += ", ";
      out += to_string(elems[i]);
    }
    out += ")";
    return out;
  }
  if (t.is_array()) {
    const auto &arr = t.as_array();
    return "[" + to_string(*arr.element) + "; <expr>]";
  }
  if (t.is_slice()) {
    const auto &sl = t.as_slice();
    return "[" + to_string(*sl.element) + "]";
  }
  if (t.is_path()) {
    const auto &segments = t.as_path().segments;
    std::string out;
    for (size_t i = 0; i < segments.size(); ++i) {
      if (i)
        out += "::";
      out += segments[i];
    }
    return out.empty() ? std::string("<path>") : out;
  }
  if (t.is_reference()) {
    const auto &ref = t.as_reference();
    return "&" + std::string(ref.is_mutable ? "mut " : "") +
           to_string(*ref.target);
  }

  return "<unknown>";
}

} // namespace rc