#pragma once

#include "../types.hpp"
#include "base.hpp"
#include "stmt.hpp"

#include <memory>
#include <vector>

namespace rc {
class FunctionDecl : public BaseNode {
public:
  std::string name;
  std::vector<std::string> parameters;
  std::vector<LiteralType> paramers_types;
  std::vector<LiteralType> return_types;
  BlockStatement *body;

  FunctionDecl(const std::string &name, const std::vector<std::string> &params,
               const std::vector<LiteralType> &param_types,
               const std::vector<LiteralType> &return_types,
               BlockStatement *body)
      : name(name), parameters(params), paramers_types(param_types),
        return_types(return_types), body(body) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class StructDecl : public BaseNode {
public:
  std::string name;
  std::vector<std::pair<std::string, std::string>> fields;

  StructDecl(const std::string &name,
             const std::vector<std::pair<std::string, std::string>> &fields)
      : name(name), fields(fields) {}

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

class RootNode : public BaseNode {
public:
  std::vector<std::unique_ptr<BaseNode>> children;

  void accept(BaseVisitor &visitor) override { visitor.visit(*this); }
};

} // namespace rc