#include "opt/analysis/dominator_tree.hpp"

#include "opt/utils/cfg_pretty_print.hpp"

#include <limits>
#include <utility>

namespace rc::opt {

namespace {

const std::vector<ir::BasicBlock *> &empty_block_vec() {
  static const std::vector<ir::BasicBlock *> kEmpty;
  return kEmpty;
}

const std::unordered_set<ir::BasicBlock *> &empty_block_set() {
  static const std::unordered_set<ir::BasicBlock *> kEmpty;
  return kEmpty;
}

} // namespace

void DominatorTree::clear() {
  idom_.clear();
  rpo_index_.clear();
  children_.clear();
  dom_frontiers_.clear();
  rpo_order_.clear();
}

void DominatorTree::compute(ir::Function &function) {
  clear();

  const auto &blocks = function.blocks();
  if (blocks.empty()) {
    return;
  }

  // Snapshot successors of every block once (CFG won't be mutated below).
  std::unordered_map<ir::BasicBlock *, std::vector<ir::BasicBlock *>> succs;
  succs.reserve(blocks.size());
  for (const auto &bb : blocks) {
    auto &succ_list = succs[bb.get()];
    for (auto *s : utils::detail::successors(*bb)) {
      succ_list.push_back(const_cast<ir::BasicBlock *>(s));
    }
  }

  // Iterative DFS from entry, recording postorder.
  auto *entry = blocks.front().get();
  std::vector<ir::BasicBlock *> postorder;
  std::unordered_set<ir::BasicBlock *> visited;
  std::vector<std::pair<ir::BasicBlock *, std::size_t>> stack;
  visited.insert(entry);
  stack.push_back({entry, 0});
  while (!stack.empty()) {
    auto &[bb, next_idx] = stack.back();
    const auto &block_succs = succs[bb];
    if (next_idx < block_succs.size()) {
      auto *s = block_succs[next_idx++];
      if (s && visited.insert(s).second) {
        stack.push_back({s, 0});
      }
      continue;
    }
    postorder.push_back(bb);
    stack.pop_back();
  }

  rpo_order_.assign(postorder.rbegin(), postorder.rend());
  for (std::size_t i = 0; i < rpo_order_.size(); ++i) {
    rpo_index_[rpo_order_[i]] = i;
    idom_[rpo_order_[i]] = nullptr;
  }

  // Cooper/Harvey/Kennedy. Sentinel: entry initially dominates itself, cleared
  // back to nullptr at the end so callers see a single "no parent" marker.
  idom_[entry] = entry;
  auto intersect = [&](ir::BasicBlock *lhs, ir::BasicBlock *rhs) {
    while (lhs != rhs) {
      while (rpo_index_[lhs] > rpo_index_[rhs]) {
        lhs = idom_[lhs];
      }
      while (rpo_index_[rhs] > rpo_index_[lhs]) {
        rhs = idom_[rhs];
      }
    }
    return lhs;
  };

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto *bb : rpo_order_) {
      if (bb == entry) {
        continue;
      }
      ir::BasicBlock *new_idom = nullptr;
      for (auto *pred : bb->predecessors()) {
        if (!rpo_index_.count(pred)) {
          continue;
        }
        if (pred != entry && !idom_[pred]) {
          continue;
        }
        new_idom = new_idom ? intersect(pred, new_idom) : pred;
      }
      if (new_idom && idom_[bb] != new_idom) {
        idom_[bb] = new_idom;
        changed = true;
      }
    }
  }
  idom_[entry] = nullptr;

  // Dominator-tree children.
  for (const auto &[bb, parent] : idom_) {
    if (bb && parent) {
      children_[parent].push_back(bb);
    }
  }

  // Dominance frontiers: for each block with multiple predecessors, walk each
  // predecessor up the idom chain until we reach the block's own idom.
  for (const auto &bb : blocks) {
    const auto &preds = bb->predecessors();
    if (preds.size() < 2) {
      continue;
    }
    for (auto *p : preds) {
      if (!rpo_index_.count(p)) {
        continue;
      }
      auto *runner = p;
      auto *stop = idom_[bb.get()];
      while (runner && runner != stop) {
        dom_frontiers_[runner].insert(bb.get());
        runner = idom_[runner];
      }
    }
  }
}

ir::BasicBlock *DominatorTree::idom(ir::BasicBlock *bb) const {
  auto it = idom_.find(bb);
  return it == idom_.end() ? nullptr : it->second;
}

const std::vector<ir::BasicBlock *> &
DominatorTree::children(ir::BasicBlock *bb) const {
  auto it = children_.find(bb);
  return it == children_.end() ? empty_block_vec() : it->second;
}

const std::unordered_set<ir::BasicBlock *> &
DominatorTree::dom_frontier(const ir::BasicBlock *bb) const {
  auto it = dom_frontiers_.find(bb);
  return it == dom_frontiers_.end() ? empty_block_set() : it->second;
}

std::size_t DominatorTree::rpo_index(ir::BasicBlock *bb) const {
  auto it = rpo_index_.find(bb);
  return it == rpo_index_.end() ? std::numeric_limits<std::size_t>::max()
                                : it->second;
}

bool DominatorTree::reaches(ir::BasicBlock *bb) const {
  return rpo_index_.count(bb) > 0;
}

bool DominatorTree::dominates(ir::BasicBlock *a, ir::BasicBlock *b) const {
  if (!a || !b) {
    return false;
  }
  if (a == b) {
    return reaches(a);
  }
  auto it = idom_.find(b);
  while (it != idom_.end() && it->second) {
    if (it->second == a) {
      return true;
    }
    it = idom_.find(it->second);
  }
  return false;
}

} // namespace rc::opt
