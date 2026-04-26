#include "ast/visitors/pretty_print.hpp"

namespace rc {

std::string pretty_print(BaseNode &node, int indent_level) {
  PrettyPrintVisitor visitor(indent_level, true);
  node.accept(visitor);
  return visitor.get_result();
}

} // namespace rc
