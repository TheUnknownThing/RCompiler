#pragma once

#include <string>
#include <stdexcept>

namespace rc {
class SemanticException : public std::runtime_error {
public:
  explicit SemanticException(const std::string &message)
      : std::runtime_error("Semantic Error: " + message) {}
};

class TypeError : public SemanticException {
public:
  explicit TypeError(const std::string &message)
      : SemanticException("Type Error: " + message) {}
};
} // namespace rc