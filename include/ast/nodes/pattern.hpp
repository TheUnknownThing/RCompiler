#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.hpp"

namespace rc {

using Path = std::vector<std::string>;

class BasePattern {
public:
  virtual ~BasePattern() = default;
  virtual void accept(class BaseVisitor &visitor) = 0;
};

class IdentifierPattern : public BasePattern {
public:
  std::string name;
  bool is_mutable;
  bool is_ref;

  IdentifierPattern(std::string n, bool ref = false, bool mutable_ = false)
      : name(std::move(n)), is_mutable(mutable_), is_ref(ref) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class LiteralPattern : public BasePattern {
public:
  std::string value;
  bool is_negative = false;

  explicit LiteralPattern(std::string v, bool negative = false)
      : value(std::move(v)), is_negative(negative) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class ReferencePattern : public BasePattern {
public:
  std::shared_ptr<BasePattern> inner_pattern;
  bool is_mutable;

  ReferencePattern(std::shared_ptr<BasePattern> inner, bool mutable_ = false)
      : inner_pattern(std::move(inner)), is_mutable(mutable_) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class OrPattern : public BasePattern {
public:
  std::vector<std::shared_ptr<BasePattern>> alternatives;
  OrPattern(std::vector<std::shared_ptr<BasePattern>> alts)
      : alternatives(std::move(alts)) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc