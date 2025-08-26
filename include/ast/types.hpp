#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace rc {

enum class PrimitiveLiteralType {
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
  UNIT
};

struct LiteralType {
  struct Tuple {
    std::vector<LiteralType> elements;
  };
  struct Array {
    std::shared_ptr<LiteralType> element;
    std::uint64_t size;
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

  using Storage =
      std::variant<PrimitiveLiteralType, Tuple, Array, Slice, Path, Union>;

  Storage storage;

  LiteralType() = default;
  explicit LiteralType(Storage s) : storage(std::move(s)) {}
  LiteralType(PrimitiveLiteralType b) : storage(b) {}

  static LiteralType base(PrimitiveLiteralType b) { return LiteralType{b}; }
  static LiteralType tuple(std::vector<LiteralType> elems) {
    return LiteralType{Tuple{std::move(elems)}};
  }
  static LiteralType array(LiteralType elem, std::uint64_t size) {
    return LiteralType{
        Array{std::make_shared<LiteralType>(std::move(elem)), size}};
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

  bool is_base() const {
    return std::holds_alternative<PrimitiveLiteralType>(storage);
  }
  bool is_tuple() const { return std::holds_alternative<Tuple>(storage); }
  bool is_array() const { return std::holds_alternative<Array>(storage); }
  bool is_slice() const { return std::holds_alternative<Slice>(storage); }
  bool is_path() const { return std::holds_alternative<Path>(storage); }

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
};

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
  return false;
}

inline bool operator<(const LiteralType &a, const LiteralType &b) {
  auto rank = [](const LiteralType &t) -> int {
    if (t.is_base())
      return 0;
    if (t.is_tuple())
      return 1;
    if (t.is_array())
      return 2;
    if (t.is_slice())
      return 3;
    if (t.is_path())
      return 4;
    return 6;
  };
  int ra = rank(a), rb = rank(b);
  if (ra != rb)
    return ra < rb;

  if (a.is_base() && b.is_base())
    return static_cast<int>(a.as_base()) < static_cast<int>(b.as_base());

  if (a.is_tuple() && b.is_tuple()) {
    const auto &ae = a.as_tuple();
    const auto &be = b.as_tuple();
    const auto n = std::min(ae.size(), be.size());
    for (size_t i = 0; i < n; ++i) {
      if (ae[i] < be[i])
        return true;
      if (be[i] < ae[i])
        return false;
    }
    return ae.size() < be.size();
  }

  if (a.is_array() && b.is_array()) {
    const auto &ae = a.as_array();
    const auto &be = b.as_array();
    if (*ae.element == *be.element)
      return ae.size < be.size;
    return *ae.element < *be.element;
  }

  if (a.is_slice() && b.is_slice()) {
    return *a.as_slice().element < *b.as_slice().element;
  }

  if (a.is_path() && b.is_path()) {
    const auto &ap = a.as_path().segments;
    const auto &bp = b.as_path().segments;
    const auto n = std::min(ap.size(), bp.size());
    for (size_t i = 0; i < n; ++i) {
      if (ap[i] < bp[i])
        return true;
      if (bp[i] < ap[i])
        return false;
    }
    return ap.size() < bp.size();
  }

  return false; // same rank but no comparable case
}

inline const std::map<std::string, PrimitiveLiteralType> base_literal_type_map =
    {{"i32", PrimitiveLiteralType::I32},
     {"u32", PrimitiveLiteralType::U32},
     {"isize", PrimitiveLiteralType::ISIZE},
     {"usize", PrimitiveLiteralType::USIZE},
     {"string", PrimitiveLiteralType::STRING},
     {"raw_string", PrimitiveLiteralType::RAW_STRING},
     {"c_string", PrimitiveLiteralType::C_STRING},
     {"raw_c_string", PrimitiveLiteralType::RAW_C_STRING},
     {"char", PrimitiveLiteralType::CHAR},
     {"bool", PrimitiveLiteralType::BOOL},
     {"never", PrimitiveLiteralType::NEVER},
     {"unit", PrimitiveLiteralType::UNIT}};

inline const std::map<PrimitiveLiteralType, std::string>
    literal_type_reverse_map = {
        {PrimitiveLiteralType::I32, "i32"},
        {PrimitiveLiteralType::U32, "u32"},
        {PrimitiveLiteralType::ISIZE, "isize"},
        {PrimitiveLiteralType::USIZE, "usize"},
        {PrimitiveLiteralType::STRING, "string"},
        {PrimitiveLiteralType::RAW_STRING, "raw_string"},
        {PrimitiveLiteralType::C_STRING, "c_string"},
        {PrimitiveLiteralType::RAW_C_STRING, "raw_c_string"},
        {PrimitiveLiteralType::CHAR, "char"},
        {PrimitiveLiteralType::BOOL, "bool"},
        {PrimitiveLiteralType::NEVER, "!"},
        {PrimitiveLiteralType::UNIT, "unit"}};

inline const std::map<std::string, LiteralType> literal_type_map = {
    {"i32", LiteralType::base(PrimitiveLiteralType::I32)},
    {"u32", LiteralType::base(PrimitiveLiteralType::U32)},
    {"isize", LiteralType::base(PrimitiveLiteralType::ISIZE)},
    {"usize", LiteralType::base(PrimitiveLiteralType::USIZE)},
    {"string", LiteralType::base(PrimitiveLiteralType::STRING)},
    {"raw_string", LiteralType::base(PrimitiveLiteralType::RAW_STRING)},
    {"c_string", LiteralType::base(PrimitiveLiteralType::C_STRING)},
    {"raw_c_string", LiteralType::base(PrimitiveLiteralType::RAW_C_STRING)},
    {"char", LiteralType::base(PrimitiveLiteralType::CHAR)},
    {"bool", LiteralType::base(PrimitiveLiteralType::BOOL)},
    {"never", LiteralType::base(PrimitiveLiteralType::NEVER)},
    {"unit", LiteralType::base(PrimitiveLiteralType::UNIT)}};

inline const std::set<std::string> valid_literal_types = {
    "i32",      "u32",          "isize", "usize", "string", "raw_string",
    "c_string", "raw_c_string", "char",  "bool",  "never",  "unit"};

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
    return "[" + to_string(*arr.element) + "; " + std::to_string(arr.size) +
           "]";
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

  return "<unknown>";
}

} // namespace rc