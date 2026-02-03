#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace rc::opt {

class ConstantContext {
public:
  ConstantContext() = default;
  ~ConstantContext() = default;

  std::shared_ptr<ir::ConstantInt> getIntConstant(int value) {
    auto it = intConstants.find(value);
    if (it != intConstants.end()) {
      return it->second;
    } else {
      auto constInt =
          ir::ConstantInt::getI32(static_cast<std::uint32_t>(value));
      intConstants[value] = constInt;
      return constInt;
    }
  }

private:
  std::unordered_map<int, std::shared_ptr<ir::ConstantInt>> intConstants;
};

} // namespace rc::opt