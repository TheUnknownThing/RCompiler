#pragma once

#include <stdexcept>

namespace rc::ir {
class IRException : public std::runtime_error {
public:
  explicit IRException(const std::string &message)
      : std::runtime_error("IR Error: " + message) {}
};
} // namespace rc::ir