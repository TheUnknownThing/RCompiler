#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"

#include <memory>
#include <array>
#include <unordered_map>
#include <unordered_set>

namespace rc::opt {

class ConstantContext {
public:
  ConstantContext() = default;
  ~ConstantContext() = default;

  std::shared_ptr<ir::ConstantInt> get_int_constant(int value,
                                                  bool is_signed = true) {
    IntConstantKey key{value, is_signed};
    auto it = int_constants.find(key);
    if (it != int_constants.end()) {
      return it->second;
    } else {
      auto const_int =
          ir::ConstantInt::get_i32(static_cast<std::uint32_t>(value), is_signed);
      int_constants[key] = const_int;
      return const_int;
    }
  }

  std::shared_ptr<ir::ConstantInt> get_bool_constant(bool value) {
    auto &slot = bool_constants[static_cast<size_t>(value)];
    if (!slot) {
      slot = ir::ConstantInt::get_i1(value);
    }
    return slot;
  }

  std::shared_ptr<ir::ConstantInt>
  get_typed_int_constant(const std::shared_ptr<const ir::IntegerType> &type,
                         std::uint64_t value) {
    if (!type) {
      throw std::invalid_argument("integer type is required");
    }

    if (type->bits() == 1) {
      return get_bool_constant(value != 0);
    }

    if (type->bits() == 32) {
      return get_int_constant(static_cast<int>(static_cast<std::uint32_t>(value)),
                              type->is_signed());
    }

    return std::make_shared<ir::ConstantInt>(
        std::make_shared<ir::IntegerType>(type->bits(), type->is_signed()),
        value);
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
  struct IntConstantKey {
    int value;
    bool is_signed;

    bool operator==(const IntConstantKey &other) const {
      return value == other.value && is_signed == other.is_signed;
    }
  };

  struct IntConstantKeyHash {
    size_t operator()(const IntConstantKey &key) const {
      return std::hash<int>{}(key.value) ^
             (std::hash<bool>{}(key.is_signed) << 1);
    }
  };

  std::array<std::shared_ptr<ir::ConstantInt>, 2> bool_constants{};
  std::unordered_map<IntConstantKey, std::shared_ptr<ir::ConstantInt>,
                     IntConstantKeyHash>
      int_constants;
  std::unordered_map<std::shared_ptr<ir::Constant>,
                     std::shared_ptr<ir::ConstantPtr>>
      ptr_constants;
};

} // namespace rc::opt
