#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace rc::opt {

class ConstantContext {
public:
  ConstantContext() = default;
  ~ConstantContext() = default;

  std::shared_ptr<ir::ConstantInt> get_int_constant(int value,
                                                  bool is_signed = true) {
    auto it = int_constants.find(value);
    if (it != int_constants.end()) {
      return it->second;
    } else {
      auto const_int =
          ir::ConstantInt::get_i32(static_cast<std::uint32_t>(value), is_signed);
      int_constants[value] = const_int;
      return const_int;
    }
  }

  std::shared_ptr<ir::ConstantInt> get_bool_constant(bool value) {
    return get_int_constant(value ? 1 : 0, true);
  }

  std::shared_ptr<ir::ConstantPtr>
  get_ptr_to_const_element(const std::shared_ptr<ir::Constant> &element) {
    auto ptr_type = std::make_shared<ir::PointerType>(element->type());
    auto base_ptr = std::make_shared<ir::ConstantPtr>(ptr_type, element);
    auto it = ptr_constants.find(base_ptr);
    if (it != ptr_constants.end()) {
      return it->second;
    } else {
      ptr_constants[base_ptr] = base_ptr;
      return base_ptr;
    }
  }

private:
  std::unordered_map<int, std::shared_ptr<ir::ConstantInt>> int_constants;
  std::unordered_map<std::shared_ptr<ir::Constant>,
                     std::shared_ptr<ir::ConstantPtr>>
      ptr_constants;
};

} // namespace rc::opt