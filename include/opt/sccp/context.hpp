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

  std::shared_ptr<ir::ConstantPtr>
  getPtrToConstElement(const std::shared_ptr<ir::Constant> &element) {
    auto ptrType = std::make_shared<ir::PointerType>(element->type());
    auto basePtr = std::make_shared<ir::ConstantPtr>(ptrType, element);
    auto it = ptrConstants.find(basePtr);
    if (it != ptrConstants.end()) {
      return it->second;
    } else {
      ptrConstants[basePtr] = basePtr;
      return basePtr;
    }
  }

private:
  std::unordered_map<int, std::shared_ptr<ir::ConstantInt>> intConstants;
  std::unordered_map<std::shared_ptr<ir::Constant>,
                     std::shared_ptr<ir::ConstantPtr>>
      ptrConstants;
};

} // namespace rc::opt