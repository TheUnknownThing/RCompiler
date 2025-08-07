#pragma once

#include "base.hpp"
#include "stmt.hpp"

#include <memory>
#include <vector>

namespace rc {
class FunctionDecl : public nc::BaseNode {
public:
  std::string name;
  std::vector<std::string> parameters;
  BlockStatement *body;

  FunctionDecl(const std::string &name, const std::vector<std::string> &params,
               BlockStatement *body)
      : name(name), parameters(params), body(body) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class StructDecl : public nc::BaseNode {
public:
  std::string name;
  std::vector<std::pair<std::string, std::string>> fields;

  StructDecl(const std::string &name,
             const std::vector<std::pair<std::string, std::string>> &fields)
      : name(name), fields(fields) {}

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

class RootNode : public nc::BaseNode {
public:
  std::vector<std::unique_ptr<nc::BaseNode>> children;

  void accept(nc::BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc