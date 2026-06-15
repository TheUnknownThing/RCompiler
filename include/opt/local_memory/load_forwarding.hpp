#pragma once

#include "ir/instructions/top_level.hpp"

namespace rc::opt {

// LoadForwardingPass: forward redundant loads within a basic block (and across
// straight-line chains of single-predecessor blocks).
//
// Patterns handled:
//   * store v, p ; ... ; load p     ->  v        (no aliasing store between)
//   * load p -> r1 ; ... ; load p   ->  use r1   (no aliasing store between)
//
// Aliasing model: pointer SSA identity, refined by tracing through GEPs to
// detect that two pointers come from distinct AllocaInsts (and therefore
// cannot alias). All other pointer pairs are assumed to potentially alias.
// Calls conservatively clobber all known values.
class LoadForwardingPass {
public:
  void run(ir::Module &module);
};

} // namespace rc::opt
