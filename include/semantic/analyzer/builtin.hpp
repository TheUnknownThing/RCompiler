#pragma once

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

inline auto add_builtin_function(const std::string &name,
                                 const std::vector<SemType> &param_types,
                                 const std::vector<std::string> &param_names,
                                 const SemType &return_type, ScopeNode *prelude,
                                 bool is_special = false) {
  auto *builtin_fn = new BuiltinFunction(name, param_types, param_names,
                                         return_type, is_special);

  prelude->add_item(name, ItemKind::Function, builtin_fn);

  auto *item = prelude->find_value_item(name);
  if (item) {
    FunctionMetaData meta;
    meta.name = name;
    meta.return_type = return_type;
    meta.param_types = param_types;
    for (const auto &param_name : param_names) {
      meta.param_names.push_back(
          std::make_shared<IdentifierPattern>(param_name, false, false));
    }
    meta.decl = nullptr;
    item->metadata = meta;
  }
};

inline ScopeNode *create_prelude_scope() {
  auto *prelude = new ScopeNode("prelude", nullptr, nullptr);

  // print(s: &str) -> ()
  add_builtin_function(
      "print", {SemType::primitive(SemPrimitiveKind::STRING)}, {"s"},
      SemType::primitive(SemPrimitiveKind::UNIT), prelude);

  // println(s: &str) -> ()
  add_builtin_function(
      "println", {SemType::primitive(SemPrimitiveKind::STRING)}, {"s"},
      SemType::primitive(SemPrimitiveKind::UNIT), prelude);

  // printInt(n: i32) -> ()
  add_builtin_function("printInt", {SemType::primitive(SemPrimitiveKind::I32)},
                       {"n"}, SemType::primitive(SemPrimitiveKind::UNIT),
                       prelude);

  // printlnInt(n: i32) -> ()
  add_builtin_function("printlnInt",
                       {SemType::primitive(SemPrimitiveKind::I32)}, {"n"},
                       SemType::primitive(SemPrimitiveKind::UNIT), prelude);

  // getString() -> String
  add_builtin_function("getString", {}, {},
                       SemType::primitive(SemPrimitiveKind::STRING), prelude);

  // getInt() -> i32
  add_builtin_function("getInt", {}, {},
                       SemType::primitive(SemPrimitiveKind::I32), prelude);

  // exit(code: i32) -> ()
  add_builtin_function("exit", {SemType::primitive(SemPrimitiveKind::I32)},
                       {"code"}, SemType::primitive(SemPrimitiveKind::UNIT),
                       prelude, true);

  return prelude;
}

inline bool is_builtin_method(const SemType &receiver_type,
                              const std::string &method_name) {
  if (receiver_type.is_primitive()) {
    auto kind = receiver_type.as_primitive().kind;

    if (method_name == "to_string") {
      return kind == SemPrimitiveKind::U32 || kind == SemPrimitiveKind::USIZE;
    }

    if (method_name == "as_str") {
      return kind == SemPrimitiveKind::STRING;
    }

    if (method_name == "len") {
      return kind == SemPrimitiveKind::STRING ||
             kind == SemPrimitiveKind::RAW_STRING;
    }
  }

  if (receiver_type.is_reference()) {
    auto &ref = receiver_type.as_reference();
    auto &target = *ref.target;

    // len method available on &[T], &str
    if (method_name == "len") {
      if (target.is_slice()) {
        return true;
      }
      if (target.is_primitive() &&
          target.as_primitive().kind == SemPrimitiveKind::RAW_STRING) {
        return true;
      }
    }
  }

  if (receiver_type.is_array()) {
    // len method available on [T; N]
    if (method_name == "len") {
      return true;
    }
  }

  if (receiver_type.is_slice()) {
    // len method available on &[T]
    if (method_name == "len") {
      return true;
    }
  }

  return false;
}

inline SemType get_builtin_method_return_type(const SemType &receiver_type,
                                              const std::string &method_name) {
  if (!is_builtin_method(receiver_type, method_name)) {
    throw SemanticException("Not a builtin method: " + method_name);
  }

  if (method_name == "to_string") {
    return SemType::primitive(SemPrimitiveKind::STRING);
  }

  if (method_name == "as_str") {
    return SemType::reference(SemType::primitive(SemPrimitiveKind::RAW_STRING),
                              false);
  }

  if (method_name == "len") {
    return SemType::primitive(SemPrimitiveKind::USIZE);
  }

  throw SemanticException("Unknown builtin method: " + method_name);
}

} // namespace rc
