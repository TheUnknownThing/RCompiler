#pragma once

#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/instructions/top_level.hpp"
#include "opt/analysis/dominator_tree.hpp"

namespace rc::opt {

// One natural loop in a function: blocks reachable backwards from any latch
// without crossing the header, plus the header itself. `preheader` is null
// until `LoopInfo::ensure_preheaders` runs and either reuses an existing
// candidate or splits the loop's incoming edges to manufacture one.
struct Loop {
  ir::BasicBlock *header = nullptr;
  ir::BasicBlock *preheader = nullptr;
  std::vector<ir::BasicBlock *> latches;
  std::unordered_set<ir::BasicBlock *> blocks;
  Loop *parent = nullptr;
  std::vector<Loop *> children;

  bool contains(ir::BasicBlock *bb) const { return blocks.count(bb) > 0; }
};

class LoopInfo {
public:
  void analyze(ir::Function &function, const DominatorTree &dt);
  void clear();

  // Ensures every loop in `function` has a dedicated preheader block. May
  // create blocks and rewire terminators / header phis; predecessors of the
  // affected blocks are kept consistent so callers do not need to revisit_cfg
  // before reading them via `bb->predecessors()` again.
  void ensure_preheaders(ir::Function &function);

  Loop *get_loop_for(ir::BasicBlock *bb) const;
  const std::vector<Loop *> &top_level_loops() const { return top_level_; }
  // Innermost-first traversal of the loop forest, the order LICM wants so
  // that inner-loop invariants are hoisted before their enclosing-loop hoists
  // are considered.
  std::vector<Loop *> loops_postorder() const;

private:
  // deque so Loop pointers remain stable as the storage grows during analyze.
  std::deque<Loop> storage_;
  std::unordered_map<ir::BasicBlock *, Loop *> header_to_loop_;
  std::unordered_map<ir::BasicBlock *, Loop *> bb_to_innermost_;
  std::vector<Loop *> top_level_;

  void postorder_walk(Loop *l, std::vector<Loop *> &out) const;
};

} // namespace rc::opt
