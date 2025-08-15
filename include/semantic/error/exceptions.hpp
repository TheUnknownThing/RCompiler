#pragma once

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

class UndefinedVariableError : public SemanticException {
public:
  explicit UndefinedVariableError(const std::string &var_name)
      : SemanticException("Undefined variable: " + var_name) {}
};

class UnresolvedPathError : public SemanticException {
public:
  explicit UnresolvedPathError(const std::string &path)
      : SemanticException("Unresolved path: " + path) {}
};

class InvalidPatternError : public SemanticException {
public:
  explicit InvalidPatternError(const std::string &pattern)
      : SemanticException("Invalid pattern: " + pattern) {}
};

class InvalidExpressionError : public SemanticException {
public:
  explicit InvalidExpressionError(const std::string &expr)
      : SemanticException("Invalid expression: " + expr) {}
};
} // namespace rc