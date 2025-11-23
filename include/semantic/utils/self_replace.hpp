#pragma once

#include <string>
#include <vector>

#include "ast/types.hpp"

namespace rc {

// Replace `Self` Type with given target name
inline LiteralType replace_self(LiteralType t, const std::string &target_name) {
  if (t.is_path()) {
    auto &segs = t.as_path().segments;
    if (segs.size() == 1 && segs[0] == "Self") {
      return LiteralType::path(std::vector<std::string>{target_name});
    }
    return t;
  }
  if (t.is_tuple()) {
    for (auto &el : t.as_tuple()) {
      el = replace_self(el, target_name);
    }
    return t;
  }
  if (t.is_array()) {
    *t.as_array().element =
        replace_self(*t.as_array().element, target_name);
    return t;
  }
  if (t.is_slice()) {
    *t.as_slice().element =
        replace_self(*t.as_slice().element, target_name);
    return t;
  }
  if (t.is_reference()) {
    *t.as_reference().target =
        replace_self(*t.as_reference().target, target_name);
    return t;
  }
  return t;
}

} // namespace rc
