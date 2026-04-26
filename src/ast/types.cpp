#include "ast/types.hpp"

namespace rc {

bool operator==(const AstType &a, const AstType &b) {
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
const std::map<PrimitiveAstType, std::string>
  literal_type_reverse_map = {
#define X(name, display, parse) {PrimitiveAstType::name, display},
#include "common/primitive_types.def"
#undef X
};
const std::map<std::string, AstType> literal_type_map = {
#define X(name, display, parse) {parse, AstType::base(PrimitiveAstType::name)},
#include "common/primitive_types.def"
#undef X
};
std::string to_string(const AstType &t) {
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
