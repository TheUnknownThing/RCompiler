#include "ir/instructions/type.hpp"

namespace rc::ir {

std::shared_ptr<Value> remap_value(const std::shared_ptr<Value> &v,
                                         const ValueRemapMap &value_map) {
  if (!v) {
    return nullptr;
  }
  auto it = value_map.find(v.get());
  if (it != value_map.end()) {
    return it->second;
  }
  return v;
}

BasicBlock *remap_block(BasicBlock *bb, const BlockRemapMap &block_map) {
  if (!bb) {
    return nullptr;
  }
  auto it = block_map.find(bb);
  if (it != block_map.end()) {
    return it->second;
  }
  return bb;
}

} // namespace rc::ir
