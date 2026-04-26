#include "semantic/types.hpp"

namespace rc {

bool operator==(const SemType &a, const SemType &b) {
  if (a.is_never() || b.is_never())
    return true;
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
std::string to_string(SemPrimitiveKind k) {
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
  case SemPrimitiveKind::STR:
    return "str";
  case SemPrimitiveKind::CHAR:
    return "char";
  case SemPrimitiveKind::BOOL:
    return "bool";
  case SemPrimitiveKind::NEVER:
    return "never!";
  case SemPrimitiveKind::UNIT:
    return "()";
  case SemPrimitiveKind::UNKNOWN:
    return "<unknown>";
  }
  return "<invalid>";
}
std::string to_string(const SemType &t) {
  if (t.is_primitive())
    return to_string(t.as_primitive().kind);
  if (t.is_tuple()) {
    const auto &els = t.as_tuple().elements;
    std::string out = "tuple(";
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
