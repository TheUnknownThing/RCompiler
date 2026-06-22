#pragma once

#include "ir/instructions/top_level.hpp"

namespace rc::opt {

// Recovers a `switch` from an if/else-if ladder. A chain of blocks each testing
// `icmp eq <scrutinee>, K_i` and branching (case-arm vs next-test) is collapsed
// into a single SwitchInst, which the backend can lower to an O(1) jump table.
// This is what makes a bytecode-VM dispatch loop (O(#opcodes) comparisons per
// dispatched instruction) tractable. The transform only fires on long, dense,
// unambiguous ladders; everything else is left untouched.
class SwitchRecovery {
public:
  void run(ir::Module &module);

private:
  bool run_on_function(ir::Function &function);
};

} // namespace rc::opt
