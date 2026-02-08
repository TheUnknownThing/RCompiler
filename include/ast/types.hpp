#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace rc {

enum class PrimitiveLiteralType {
#define X(name, display, parse) name,
#include "common/primitive_types.def"
#undef X
};

class Expression;

struct LiteralType {
  struct Tuple {
    std::vector<LiteralType> elements;
  };
  struct Array {
    std::shared_ptr<LiteralType> element;
    std::shared_ptr<Expression> size;

    int64_t actual_size = -1; // this size can only be set in const eval
  };
  struct Slice {
    std::shared_ptr<LiteralType> element;
  };
  struct Path {
    std::vector<std::string> segments;
  };
  struct Union {
    std::vector<LiteralType> alternatives;
  };
  struct Reference {
    std::shared_ptr<LiteralType> target;
    bool is_mutable;
  };

  using Storage = std::variant<PrimitiveLiteralType, Tuple, Array, Slice, Path,
                               Union, Reference>;

  Storage storage;

  LiteralType() = default;
  explicit LiteralType(Storage s) : storage(std::move(s)) {}
  LiteralType(PrimitiveLiteralType b) : storage(b) {}

  static LiteralType base(PrimitiveLiteralType b) { return LiteralType{b}; }
  static LiteralType tuple(std::vector<LiteralType> elems) {
    return LiteralType{Tuple{std::move(elems)}};
  }
  static LiteralType array(LiteralType elem,
                           std::shared_ptr<Expression> size_expr) {
    return LiteralType{Array{std::make_shared<LiteralType>(std::move(elem)),
                             std::move(size_expr)}};
  }
  static LiteralType slice(LiteralType elem) {
    return LiteralType{Slice{std::make_shared<LiteralType>(std::move(elem))}};
  }
  static LiteralType path(std::vector<std::string> segments) {
    return LiteralType{Path{std::move(segments)}};
  }
  static LiteralType union_of(std::vector<LiteralType> alts) {
    return LiteralType{Union{std::move(alts)}};
  }
  static LiteralType reference(LiteralType target, bool is_mutable) {
    return LiteralType{Reference{
        std::make_shared<LiteralType>(std::move(target)), is_mutable}};
  }

  bool is_base() const {
    return std::holds_alternative<PrimitiveLiteralType>(storage);
  }
  bool is_tuple() const { return std::holds_alternative<Tuple>(storage); }
  bool is_array() const { return std::holds_alternative<Array>(storage); }
  bool is_slice() const { return std::holds_alternative<Slice>(storage); }
  bool is_path() const { return std::holds_alternative<Path>(storage); }
  bool is_reference() const {
    return std::holds_alternative<Reference>(storage);
  }

  PrimitiveLiteralType as_base() const {
    return std::get<PrimitiveLiteralType>(storage);
  }
  const std::vector<LiteralType> &as_tuple() const {
    return std::get<Tuple>(storage).elements;
  }
  std::vector<LiteralType> &as_tuple() {
    return std::get<Tuple>(storage).elements;
  }
  const Array &as_array() const { return std::get<Array>(storage); }
  Array &as_array() { return std::get<Array>(storage); }
  const Slice &as_slice() const { return std::get<Slice>(storage); }
  Slice &as_slice() { return std::get<Slice>(storage); }
  const Path &as_path() const { return std::get<Path>(storage); }
  Path &as_path() { return std::get<Path>(storage); }
  const std::vector<LiteralType> &as_union() const {
    return std::get<Union>(storage).alternatives;
  }
  std::vector<LiteralType> &as_union() {
    return std::get<Union>(storage).alternatives;
  }
  const Reference &as_reference() const { return std::get<Reference>(storage); }
  Reference &as_reference() { return std::get<Reference>(storage); }
};

using AstType = LiteralType;

inline bool operator==(const LiteralType &a, const LiteralType &b) {
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

inline const std::map<PrimitiveLiteralType, std::string>
  literal_type_reverse_map = {
#define X(name, display, parse) {PrimitiveLiteralType::name, display},
#include "common/primitive_types.def"
#undef X
};

inline const std::map<std::string, LiteralType> literal_type_map = {
#define X(name, display, parse) {parse, LiteralType::base(PrimitiveLiteralType::name)},
#include "common/primitive_types.def"
#undef X
};

inline std::string to_string(const LiteralType &t) {
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