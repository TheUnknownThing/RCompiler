#pragma once

#include <map>
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
  TO_BE_INFERRED
};

struct LiteralType {
  struct Tuple {
    std::vector<LiteralType> elements;
  };
  using Storage = std::variant<PrimitiveLiteralType, Tuple>;

  Storage storage;

  LiteralType() = default;
  explicit LiteralType(Storage s) : storage(std::move(s)) {}
  LiteralType(PrimitiveLiteralType b) : storage(b) {}

  static LiteralType base(PrimitiveLiteralType b) {
    return LiteralType{Storage{b}};
  }
  static LiteralType tuple(std::vector<LiteralType> elems) {
    return LiteralType{Storage{Tuple{std::move(elems)}}};
  }

  bool is_base() const {
    return std::holds_alternative<PrimitiveLiteralType>(storage);
  }
  bool is_tuple() const { return std::holds_alternative<Tuple>(storage); }

  PrimitiveLiteralType as_base() const {
    return std::get<PrimitiveLiteralType>(storage);
  }
  const std::vector<LiteralType> &as_tuple() const {
    return std::get<Tuple>(storage).elements;
  }
  std::vector<LiteralType> &as_tuple() {
    return std::get<Tuple>(storage).elements;
  }
};

inline bool operator==(const LiteralType &a, const LiteralType &b) {
  if (a.is_base() && b.is_base())
    return a.as_base() == b.as_base();
  if (a.is_tuple() && b.is_tuple())
    return a.as_tuple() == b.as_tuple();
  return false;
}
inline bool operator<(const LiteralType &a, const LiteralType &b) {
  if (a.is_base() && b.is_base())
    return static_cast<int>(a.as_base()) < static_cast<int>(b.as_base());
  if (a.is_base() && b.is_tuple())
    return true;
  if (a.is_tuple() && b.is_base())
    return false;
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
     {"to_be_inferred", PrimitiveLiteralType::TO_BE_INFERRED}};

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
        {PrimitiveLiteralType::TO_BE_INFERRED, "to_be_inferred"}};
;

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
    {"to_be_inferred",
     LiteralType::base(PrimitiveLiteralType::TO_BE_INFERRED)}};

inline const std::set<std::string> valid_literal_types = {
    "i32",        "u32",      "isize",        "usize", "string",
    "raw_string", "c_string", "raw_c_string", "char",  "to_be_inferred"};

inline std::string to_string(const LiteralType &t) {
  if (t.is_base()) {
    auto it = literal_type_reverse_map.find(t.as_base());
    return it != literal_type_reverse_map.end() ? it->second : "<unknown>";
  }
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

} // namespace rc