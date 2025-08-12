#pragma once

#include <memory>
#include <optional>
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

  IdentifierPattern(std::string n, bool mutable_ = false, bool ref = false)
      : name(std::move(n)), is_mutable(mutable_), is_ref(ref) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class LiteralPattern : public BasePattern {
public:
  std::string value;

  explicit LiteralPattern(std::string v) : value(std::move(v)) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class WildcardPattern : public BasePattern {
public:
  WildcardPattern() {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class RestPattern : public BasePattern {
public:
  RestPattern() {}

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

class StructPattern : public BasePattern {
public:
  struct StructPattenField {
    std::string name;
    std::shared_ptr<BasePattern> pattern;

    StructPattenField(std::string n, std::shared_ptr<BasePattern> p)
        : name(std::move(n)), pattern(std::move(p)) {}
  };
  Path path;
  std::vector<StructPattenField> fields;
  bool has_rest;

  StructPattern(Path p, std::vector<StructPattenField> f, bool rest = false)
      : path(std::move(p)), fields(std::move(f)), has_rest(rest) {}
  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class TuplePattern : public BasePattern {
public:
  std::vector<std::shared_ptr<BasePattern>> elements;

  TuplePattern(std::vector<std::shared_ptr<BasePattern>> elems)
      : elements(std::move(elems)) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class GroupedPattern : public BasePattern {
public:
  std::shared_ptr<BasePattern> inner_pattern;

  GroupedPattern(std::shared_ptr<BasePattern> inner)
      : inner_pattern(std::move(inner)) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class PathPattern : public BasePattern {
public:
  Path path;

  PathPattern(Path p) : path(std::move(p)) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

class SlicePattern : public BasePattern {
public:
  std::vector<std::shared_ptr<BasePattern>> elements;

  SlicePattern(std::vector<std::shared_ptr<BasePattern>> elems)
      : elements(std::move(elems)) {}

  void accept(class BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc