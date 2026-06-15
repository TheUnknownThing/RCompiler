#include "opt/local_memory/load_forwarding.hpp"

#include "ir/instructions/binary.hpp"
#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "opt/utils/ir_utils.hpp"

#include <unordered_map>
#include <vector>

namespace rc::opt {

namespace {

using AvailMap = std::unordered_map<ir::Value *, ir::Value *>;

// Walk up GEPs until we either hit an AllocaInst (the origin) or a non-GEP
// pointer source we can't analyze further. Returns the AllocaInst* (or nullptr
// if not reachable from a known alloca).
ir::AllocaInst *trace_to_alloca(ir::Value *p) {
  for (int guard = 0; p && guard < 64; ++guard) {
    if (auto *a = dynamic_cast<ir::AllocaInst *>(p)) {
      return a;
    }
    if (auto *g = dynamic_cast<ir::GetElementPtrInst *>(p)) {
      p = g->base_pointer().get();
      continue;
    }
    return nullptr;
  }
  return nullptr;
}

bool may_alias(ir::Value *p, ir::Value *q) {
  if (p == q) {
    return true;
  }
  auto *ap = trace_to_alloca(p);
  auto *aq = trace_to_alloca(q);
  if (ap && aq && ap != aq) {
    return false;
  }
  return true;
}

// Two values have compatible types for load/store forwarding when they share
// identical layout (we use this to refuse forwarding across signed/unsigned or
// width mismatches, even though they'd be rare in practice).
bool types_compatible(const ir::TypePtr &a, const ir::TypePtr &b) {
  if (a == b) {
    return true;
  }
  if (!a || !b) {
    return false;
  }
  if (a->kind() != b->kind()) {
    return false;
  }
  if (auto ai = std::dynamic_pointer_cast<const ir::IntegerType>(a)) {
    auto bi = std::dynamic_pointer_cast<const ir::IntegerType>(b);
    return bi && ai->bits() == bi->bits();
  }
  if (a->kind() == ir::TypeKind::Pointer) {
    return true;
  }
  return false;
}

void kill_aliasing(AvailMap &avail, ir::Value *p) {
  for (auto it = avail.begin(); it != avail.end();) {
    if (it->first != p && may_alias(it->first, p)) {
      it = avail.erase(it);
    } else {
      ++it;
    }
  }
}

// Process one basic block given an initial "available" map (carried in from a
// single predecessor when applicable). Returns the available map at block exit
// and sets `changed` if any load was forwarded.
AvailMap run_on_block(ir::BasicBlock &bb, AvailMap avail, bool &changed) {
  // Snapshot the instruction list because we erase as we walk.
  auto insts = bb.instructions();
  for (auto &inst_sp : insts) {
    auto *inst = inst_sp.get();
    if (!inst) {
      continue;
    }

    if (auto *ld = dynamic_cast<ir::LoadInst *>(inst)) {
      auto *p = ld->pointer().get();
      auto it = avail.find(p);
      if (it != avail.end() && it->second &&
          types_compatible(it->second->type(), ld->type())) {
        auto *known = it->second;
        utils::replace_all_uses_with(*ld, known);
        utils::erase_instruction(bb, ld);
        changed = true;
        continue;
      }
      avail[p] = ld;
      continue;
    }

    if (auto *st = dynamic_cast<ir::StoreInst *>(inst)) {
      auto *p = st->pointer().get();
      auto *v = st->value().get();
      kill_aliasing(avail, p);
      // Type compatibility is verified at the load site, not here.
      avail[p] = v;
      continue;
    }

    if (dynamic_cast<ir::CallInst *>(inst)) {
      // A call may write to any memory we don't know about. Drop everything
      // whose root pointer we can't see as a local alloca (those local
      // allocas could still escape through the call's pointer arguments,
      // so we drop them too - conservatively clear everything).
      avail.clear();
      continue;
    }

    // Phi, Branch, Return, BinaryOp, ICmp, GEP, Sext/Zext/Trunc, Select,
    // Alloca, MoveInst, Unreachable: none of these affect known load values
    // at any pointer in the map. (GEPs may *define* new pointer values, but
    // they don't store anything.)
  }
  return avail;
}

} // namespace

void LoadForwardingPass::run(ir::Module &module) {
  for (const auto &fn : module.functions()) {
    if (!fn || fn->is_external()) {
      continue;
    }

    // Compute exit-available maps so a single-pred block can inherit straight
    // through from its predecessor.
    std::unordered_map<ir::BasicBlock *, AvailMap> exit_avail;

    // Process blocks in the order they appear in the function. The IR emitter
    // emits roughly in reverse postorder, so single-pred chains will be
    // visited in chain order most of the time.
    for (auto &bb : fn->blocks()) {
      if (!bb) {
        continue;
      }

      AvailMap entry;
      // If we have exactly one predecessor and we've already processed it,
      // start from its exit set.
      const auto &preds = bb->predecessors();
      if (preds.size() == 1 && preds.front() != bb.get()) {
        auto it = exit_avail.find(preds.front());
        if (it != exit_avail.end()) {
          entry = it->second;
        }
      }

      bool changed_local = false;
      auto exit = run_on_block(*bb, std::move(entry), changed_local);
      (void)changed_local;
      exit_avail[bb.get()] = std::move(exit);
    }
  }
}

} // namespace rc::opt
