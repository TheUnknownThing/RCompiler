#pragma once

#include <memory>
#include <string>
#include <vector>

#include "semantic/scope.hpp"
#include "semantic/types.hpp"

namespace rc {

class BuiltinFunction : public BaseItem {
public:
  std::string name;
  std::vector<SemType> param_types;
  std::vector<std::string> param_names;
  SemType return_type;
  bool is_special; // exit(), special, only allowed in main function

  BuiltinFunction(std::string name, std::vector<SemType> param_types,
                  std::vector<std::string> param_names, SemType return_type,
                  bool is_special = false)
      : name(std::move(name)), param_types(std::move(param_types)),
        param_names(std::move(param_names)),
        return_type(std::move(return_type)), is_special(is_special) {}

  void accept(BaseVisitor &visitor) override { (void)visitor; }
};

void add_builtin_function(const std::string &name,
                                 const std::vector<SemType> &param_types,
                                 const std::vector<std::string> &param_names,
                                 const SemType &return_type, ScopeNode *prelude,
                                 bool is_special = false);

std::unique_ptr<ScopeNode> create_prelude_scope();

bool is_builtin_method(const SemType &receiver_type,
                              const std::string &method_name);

SemType get_builtin_method_return_type(const SemType &receiver_type,
                                              const std::string &method_name);

} // namespace rc
