#include "opt/licm/licm.hpp"

#include "ir/instructions/binary.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "opt/analysis/dominator_tree.hpp"
#include "opt/analysis/loop_info.hpp"

#include <vector>

namespace rc::opt {

namespace {

// Conservative purity. Anything not on this list is left in place; this skips
// loads/stores/calls (no alias analysis) and integer divs (a divisor that is
// zero only on a non-taken path would trap if the div hoisted above its guard).
bool is_safe_to_hoist(const ir::Instruction &inst) {
  if (auto *b = dynamic_cast<const ir::BinaryOpInst *>(&inst)) {
    switch (b->op()) {
    case ir::BinaryOpKind::SDIV:
    case ir::BinaryOpKind::UDIV:
    case ir::BinaryOpKind::SREM:
    case ir::BinaryOpKind::UREM:
      return false;
    default:
      return true;
    }
  }
  return dynamic_cast<const ir::ICmpInst *>(&inst) ||
         dynamic_cast<const ir::GetElementPtrInst *>(&inst) ||
         dynamic_cast<const ir::ZExtInst *>(&inst) ||
         dynamic_cast<const ir::SExtInst *>(&inst) ||
         dynamic_cast<const ir::TruncInst *>(&inst) ||
         dynamic_cast<const ir::SelectInst *>(&inst);
}

bool is_invariant(const ir::Instruction &inst, const Loop &loop) {
  for (auto *op : inst.get_operands()) {
    if (!op) {
      continue;
    }
    if (dynamic_cast<ir::Constant *>(op)) {
      continue;
    }
    if (dynamic_cast<ir::Argument *>(op)) {
      continue;
    }
    if (auto *defined = dynamic_cast<ir::Instruction *>(op)) {
      if (defined->parent() && !loop.contains(defined->parent())) {
        continue; // defined outside the loop (incl. already hoisted)
      }
    }
    return false;
  }
  return true;
}

void hoist_loop(Loop &loop) {
  if (!loop.preheader) {
    return;
  }
  // We need a terminator in the preheader to insert before; ensure_preheaders
  // guarantees a single BranchInst.
  if (loop.preheader->instructions().empty()) {
    return;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto *bb : loop.blocks) {
      // Snapshot — entries get spliced out as we hoist.
      auto snapshot = bb->instructions();
      for (auto &inst : snapshot) {
        if (!inst) {
          continue;
        }
        if (inst->parent() != bb) {
          continue; // already moved this iteration
        }
        if (inst->is_terminator()) {
          continue;
        }
        if (dynamic_cast<ir::PhiInst *>(inst.get())) {
          continue;
        }
        if (!is_safe_to_hoist(*inst) || !is_invariant(*inst, loop)) {
          continue;
        }
        auto *cur = inst->parent();
        cur->detach_instruction(inst);
        auto term = loop.preheader->instructions().back();
        loop.preheader->insert_existing_before(term, inst);
        changed = true;
      }
    }
  }
}

void collect_preorder(Loop *l, std::vector<Loop *> &out) {
  out.push_back(l);
  for (auto *c : l->children) {
    collect_preorder(c, out);
  }
}

} // namespace

void LICMPass::run(ir::Module &module) {
  for (const auto &fn : module.functions()) {
    if (!fn || fn->is_external()) {
      continue;
    }

    DominatorTree dt;
    dt.compute(*fn);

    LoopInfo li;
    li.analyze(*fn, dt);
    li.ensure_preheaders(*fn);

    // Outermost-first preorder: a value that is invariant in an outer loop is
    // hoisted to the outer preheader (outside the inner loop too); the inner
    // pass then only needs to consider strictly inner-loop-invariant values.
    std::vector<Loop *> order;
    for (auto *l : li.top_level_loops()) {
      collect_preorder(l, order);
    }
    for (auto *l : order) {
      hoist_loop(*l);
    }
  }
}

} // namespace rc::opt
