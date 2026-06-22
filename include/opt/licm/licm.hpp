#pragma once

#include "ir/instructions/top_level.hpp"

namespace rc::opt {

// Loop-invariant code motion. For each natural loop, hoists pure instructions
// whose operands are all defined outside the loop (or already hoisted) into
// the loop's preheader. The first version only handles arithmetic / icmp /
// gep / casts / select — no loads/stores/calls (no alias analysis yet) and no
// integer divs (their trap on zero must not move above its guard).
class LICMPass {
public:
  void run(ir::Module &module);
};

} // namespace rc::opt
