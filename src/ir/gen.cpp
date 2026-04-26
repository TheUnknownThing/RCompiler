#include "ir/gen.hpp"

namespace rc::ir {

void emit_llvm(const Module &mod, std::ostream &out) {
  LLVMEmitter emitter(out);
  emitter.emit_module(mod);
}

} // namespace rc::ir
