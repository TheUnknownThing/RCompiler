#include "ir/gen.hpp"

namespace rc::ir {

void emitLLVM(const Module &mod, std::ostream &out) {
  LLVMEmitter emitter(out);
  emitter.emitModule(mod);
}

} // namespace rc::ir
