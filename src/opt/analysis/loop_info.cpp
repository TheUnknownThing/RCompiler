#include "opt/analysis/loop_info.hpp"

#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/misc.hpp"
#include "opt/utils/cfg_pretty_print.hpp"

namespace rc::opt {

void LoopInfo::clear() {
  storage_.clear();
  header_to_loop_.clear();
  bb_to_innermost_.clear();
  top_level_.clear();
}

void LoopInfo::analyze(ir::Function &function, const DominatorTree &dt) {
  clear();

  // Step 1: identify back edges (A -> B where B dominates A). Group by header.
  std::unordered_map<ir::BasicBlock *, std::vector<ir::BasicBlock *>>
      header_to_latches;
  for (const auto &bb : function.blocks()) {
    ir::BasicBlock *a = bb.get();
    if (!dt.reaches(a)) {
      continue;
    }
    for (auto *s : utils::detail::successors(*bb)) {
      auto *b = const_cast<ir::BasicBlock *>(s);
      if (dt.dominates(b, a)) {
        header_to_latches[b].push_back(a);
      }
    }
  }

  // Step 2: for each header, the loop body is the set of blocks that can
  // reach any latch without passing through the header. Build by reverse
  // worklist starting from the latches.
  for (auto &[header, latches] : header_to_latches) {
    storage_.emplace_back();
    Loop &loop = storage_.back();
    loop.header = header;
    loop.latches = latches;
    loop.blocks.insert(header);

    std::vector<ir::BasicBlock *> worklist;
    for (auto *l : latches) {
      if (loop.blocks.insert(l).second) {
        worklist.push_back(l);
      }
    }
    while (!worklist.empty()) {
      auto *bb = worklist.back();
      worklist.pop_back();
      if (bb == header) {
        continue;
      }
      for (auto *p : bb->predecessors()) {
        if (!dt.reaches(p)) {
          continue;
        }
        if (loop.blocks.insert(p).second) {
          worklist.push_back(p);
        }
      }
    }

    header_to_loop_[header] = &loop;
  }

  // Step 3: innermost loop per block. Iterating in any order — for each block
  // in a loop's body, the smallest containing loop wins.
  for (auto &loop : storage_) {
    for (auto *bb : loop.blocks) {
      auto it = bb_to_innermost_.find(bb);
      if (it == bb_to_innermost_.end() ||
          loop.blocks.size() < it->second->blocks.size()) {
        bb_to_innermost_[bb] = &loop;
      }
    }
  }

  // Step 4: nesting. Loop L's parent is the smallest other loop whose body
  // contains L's header (L itself trivially does; we want a strictly enclosing
  // loop).
  for (auto &loop : storage_) {
    Loop *best = nullptr;
    for (auto &other : storage_) {
      if (&other == &loop) {
        continue;
      }
      if (other.blocks.count(loop.header) == 0) {
        continue;
      }
      if (!best || other.blocks.size() < best->blocks.size()) {
        best = &other;
      }
    }
    loop.parent = best;
  }
  for (auto &loop : storage_) {
    if (loop.parent) {
      loop.parent->children.push_back(&loop);
    } else {
      top_level_.push_back(&loop);
    }
  }
}

Loop *LoopInfo::get_loop_for(ir::BasicBlock *bb) const {
  auto it = bb_to_innermost_.find(bb);
  return it == bb_to_innermost_.end() ? nullptr : it->second;
}

void LoopInfo::postorder_walk(Loop *l, std::vector<Loop *> &out) const {
  for (auto *c : l->children) {
    postorder_walk(c, out);
  }
  out.push_back(l);
}

std::vector<Loop *> LoopInfo::loops_postorder() const {
  std::vector<Loop *> out;
  for (auto *l : top_level_) {
    postorder_walk(l, out);
  }
  return out;
}

void LoopInfo::ensure_preheaders(ir::Function &function) {
  // Iterate over a snapshot — we may add new blocks (preheaders) during the
  // walk and must not visit them as headers.
  std::vector<Loop *> loops;
  for (auto &l : storage_) {
    loops.push_back(&l);
  }

  for (auto *loop : loops) {
    ir::BasicBlock *header = loop->header;
    // Outside predecessors = those not inside the loop body (i.e., not latches
    // or other loop-internal blocks reaching the header).
    std::vector<ir::BasicBlock *> outside_preds;
    for (auto *p : header->predecessors()) {
      if (loop->blocks.count(p) == 0) {
        outside_preds.push_back(p);
      }
    }

    if (outside_preds.empty()) {
      // Entry-only loop (e.g. the function entry is its own header reached
      // solely via the latch); no preheader can dominate it.
      continue;
    }

    // Reuse case: a single outside pred whose sole successor is the header is
    // already a preheader.
    if (outside_preds.size() == 1) {
      auto *p = outside_preds.front();
      auto succs = utils::detail::successors(*p);
      if (succs.size() == 1 && succs.front() == header) {
        loop->preheader = p;
        continue;
      }
    }

    // Otherwise materialize one. Create the preheader block, rewire each
    // outside pred's terminator to it, and patch header phis.
    auto preheader_sp = function.create_block(header->name() + ".preheader");
    auto *preheader = preheader_sp.get();
    preheader->append<ir::BranchInst>(header);
    loop->preheader = preheader;

    for (auto *p : outside_preds) {
      auto term = p->instructions().back();
      if (auto *br = dynamic_cast<ir::BranchInst *>(term.get())) {
        br->replace_block(header, preheader);
      } else if (auto *sw = dynamic_cast<ir::SwitchInst *>(term.get())) {
        sw->replace_block(header, preheader);
      } else {
        continue; // not a branch — leave alone
      }
      header->remove_predecessor(p);
      preheader->add_predecessor(p);
    }
    header->add_predecessor(preheader);

    // Phi fixup. Each phi in the header has incomings from every predecessor;
    // the outside-pred incomings must now funnel through the preheader.
    const bool need_merge_phi = outside_preds.size() > 1;
    std::unordered_set<ir::BasicBlock *> outside_set(outside_preds.begin(),
                                                     outside_preds.end());

    for (const auto &inst : header->instructions()) {
      auto *phi = dynamic_cast<ir::PhiInst *>(inst.get());
      if (!phi) {
        break;
      }
      if (!need_merge_phi) {
        // Single outside pred: just relabel its incoming.
        phi->replace_incoming_block(outside_preds.front(), preheader);
        continue;
      }
      // Multiple outside preds: build a merging phi in the preheader.
      std::vector<ir::PhiInst::Incoming> merged;
      merged.reserve(outside_preds.size());
      for (const auto &inc : phi->incomings()) {
        if (outside_set.count(inc.second)) {
          merged.emplace_back(inc.first, inc.second);
        }
      }
      if (merged.empty()) {
        continue; // defensive — shouldn't happen given we found outside preds
      }
      auto merge_phi = preheader->prepend<ir::PhiInst>(
          phi->type(), merged, header->name() + ".phmerge");
      for (auto *p : outside_preds) {
        phi->remove_incoming_block(p);
      }
      phi->add_incoming(merge_phi, preheader);
    }
  }
}

} // namespace rc::opt
