#include "ir/instructions/type.hpp"

namespace rc::ir {

std::shared_ptr<Value> remapValue(const std::shared_ptr<Value> &v,
                                         const ValueRemapMap &valueMap) {
  if (!v) {
    return nullptr;
  }
  auto it = valueMap.find(v.get());
  if (it != valueMap.end()) {
    return it->second;
  }
  return v;
}
BasicBlock *remapBlock(BasicBlock *bb, const BlockRemapMap &blockMap) {
  if (!bb) {
    return nullptr;
  }
  auto it = blockMap.find(bb);
  if (it != blockMap.end()) {
    return it->second;
  }
  return bb;
}

} // namespace rc::ir
