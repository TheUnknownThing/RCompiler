#pragma once

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/instructions/top_level.hpp"

namespace rc::opt {

// Function-level dominator-tree analysis. Computes immediate dominators (via
// Cooper/Harvey/Kennedy on the reverse postorder of the CFG), the dominator
// tree (parent -> children), and dominance frontiers. Reads only the CFG; the
// caller must ensure predecessor sets are up to date (run `revisit_cfg`).
class DominatorTree {
public:
  void compute(ir::Function &function);
  void clear();

  // Immediate dominator, or nullptr for the entry / unreachable blocks.
  ir::BasicBlock *idom(ir::BasicBlock *bb) const;

  // Children in the dominator tree.
  const std::vector<ir::BasicBlock *> &children(ir::BasicBlock *bb) const;

  // Dominance frontier — blocks where `bb`'s dominance ends but it is still
  // a predecessor along some incoming edge. Used to place phi nodes.
  const std::unordered_set<ir::BasicBlock *> &
  dom_frontier(const ir::BasicBlock *bb) const;

  // Reverse-postorder index, or std::numeric_limits<std::size_t>::max() for
  // blocks unreachable from entry.
  std::size_t rpo_index(ir::BasicBlock *bb) const;
  bool reaches(ir::BasicBlock *bb) const;

  // True when `a` dominates `b` (every path from entry to `b` goes through
  // `a`). Both must be reachable; an unreachable `b` is dominated by nothing.
  bool dominates(ir::BasicBlock *a, ir::BasicBlock *b) const;

  // Reverse postorder of the reachable blocks (entry first), suitable for
  // dataflow passes that want to visit dominators before their dominatees.
  const std::vector<ir::BasicBlock *> &rpo_order() const { return rpo_order_; }

private:
  std::unordered_map<ir::BasicBlock *, ir::BasicBlock *> idom_;
  std::unordered_map<ir::BasicBlock *, std::size_t> rpo_index_;
  std::unordered_map<ir::BasicBlock *, std::vector<ir::BasicBlock *>> children_;
  std::unordered_map<const ir::BasicBlock *,
                     std::unordered_set<ir::BasicBlock *>>
      dom_frontiers_;
  std::vector<ir::BasicBlock *> rpo_order_;
};

} // namespace rc::opt
