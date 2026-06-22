#include "opt/switch_recovery/switch_recovery.hpp"

#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/misc.hpp"
#include "opt/utils/cfg_pretty_print.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rc::opt {

namespace {

// Minimum number of cases before recovery is worthwhile. Shorter ladders gain
// nothing (the backend lowers a small switch back to a comparison chain) and
// would only add churn.
constexpr std::size_t kMinCases = 4;

// A single matched `icmp eq <scrutinee>, K; br <case>, <next>` test.
struct TestMatch {
  ir::Value *scrutinee = nullptr;             // identity of the compared value
  std::shared_ptr<ir::Value> scrutinee_sp;    // owning ref for the SwitchInst
  std::shared_ptr<ir::ConstantInt> key;       // the case constant K
  ir::BasicBlock *arm = nullptr;              // branch taken on equality
  ir::BasicBlock *next = nullptr;            // branch taken otherwise
  std::shared_ptr<ir::Instruction> icmp;     // the comparison (single-use)
  std::shared_ptr<ir::Instruction> branch;   // the conditional branch
};

bool match_test(ir::BasicBlock *bb, TestMatch &out) {
  if (!bb || bb->instructions().empty()) {
    return false;
  }
  auto branch = bb->instructions().back();
  auto *br = dynamic_cast<ir::BranchInst *>(branch.get());
  if (!br || !br->is_conditional() || !br->cond()) {
    return false;
  }
  auto *icmp = dynamic_cast<ir::ICmpInst *>(br->cond().get());
  if (!icmp || icmp->pred() != ir::ICmpPred::EQ || icmp->parent() != bb) {
    return false;
  }
  // The comparison must feed only this branch, so removing the branch makes it
  // dead and the fold introduces no extra computation.
  const auto &uses = icmp->get_uses();
  if (uses.size() != 1 || uses.front() != br) {
    return false;
  }

  std::shared_ptr<ir::ConstantInt> key;
  std::shared_ptr<ir::Value> scrutinee;
  if (auto c = std::dynamic_pointer_cast<ir::ConstantInt>(icmp->lhs())) {
    key = c;
    scrutinee = icmp->rhs();
  } else if (auto c = std::dynamic_pointer_cast<ir::ConstantInt>(icmp->rhs())) {
    key = c;
    scrutinee = icmp->lhs();
  } else {
    return false;
  }

  // Locate the icmp's owning shared_ptr in the block.
  std::shared_ptr<ir::Instruction> icmp_sp;
  for (const auto &inst : bb->instructions()) {
    if (inst.get() == icmp) {
      icmp_sp = inst;
      break;
    }
  }
  if (!icmp_sp) {
    return false;
  }

  out.scrutinee = scrutinee.get();
  out.scrutinee_sp = std::move(scrutinee);
  out.key = std::move(key);
  out.arm = br->dest();
  out.next = br->alt_dest();
  out.icmp = std::move(icmp_sp);
  out.branch = std::move(branch);
  return true;
}

// A continuation block in the chain must be a *pure* single-predecessor test
// block: exactly the comparison and its branch, so folding it removes the whole
// block.
bool is_pure_test_block(ir::BasicBlock *bb,
                        const std::unordered_map<ir::BasicBlock *, int>
                            &pred_count) {
  if (!bb || bb->instructions().size() != 2) {
    return false;
  }
  auto it = pred_count.find(bb);
  return it != pred_count.end() && it->second == 1;
}

std::unordered_map<ir::BasicBlock *, int>
compute_pred_counts(ir::Function &function) {
  std::unordered_map<ir::BasicBlock *, int> counts;
  for (const auto &bb : function.blocks()) {
    for (auto *succ : utils::detail::successors(*bb)) {
      counts[succ]++;
    }
  }
  return counts;
}

} // namespace

void SwitchRecovery::run(ir::Module &module) {
  for (const auto &function : module.functions()) {
    if (!function || function->is_external()) {
      continue;
    }
    // Each successful recovery removes blocks, so restart the scan until no
    // further chain is found.
    while (run_on_function(*function)) {
    }
  }
}

bool SwitchRecovery::run_on_function(ir::Function &function) {
  auto pred_count = compute_pred_counts(function);

  for (const auto &bb_sp : function.blocks()) {
    ir::BasicBlock *head = bb_sp.get();

    TestMatch head_match;
    if (!match_test(head, head_match)) {
      continue;
    }

    ir::Value *scrutinee = head_match.scrutinee;
    std::vector<ir::SwitchInst::Case> cases;
    std::vector<ir::BasicBlock *> test_blocks{head};
    std::vector<ir::BasicBlock *> remove_blocks;
    // (target block, its current predecessor) edges that move to `head`.
    std::vector<std::pair<ir::BasicBlock *, ir::BasicBlock *>> phi_fixups;
    std::unordered_set<std::uint64_t> seen_keys;

    cases.emplace_back(head_match.key, head_match.arm);
    seen_keys.insert(head_match.key->value());

    ir::BasicBlock *cur_next = head_match.next;
    ir::BasicBlock *last_test = head;

    while (cur_next && is_pure_test_block(cur_next, pred_count)) {
      TestMatch m;
      if (!match_test(cur_next, m) || m.scrutinee != scrutinee) {
        break;
      }
      if (!seen_keys.insert(m.key->value()).second) {
        break; // duplicate constant: stop (first match already wins)
      }
      cases.emplace_back(m.key, m.arm);
      phi_fixups.emplace_back(m.arm, cur_next);
      remove_blocks.push_back(cur_next);
      test_blocks.push_back(cur_next);
      last_test = cur_next;
      cur_next = m.next;
    }

    ir::BasicBlock *default_block = cur_next;
    if (!default_block || cases.size() < kMinCases) {
      continue;
    }
    // The default's incoming edge also moves from the last test block to head.
    phi_fixups.emplace_back(default_block, last_test);

    // Safety: all targets (arms + default) must be distinct and disjoint from
    // the folded test blocks, so every moved edge is an unambiguous 1:1 rewrite
    // (a duplicate target would need two distinct `head` phi incomings).
    std::unordered_set<ir::BasicBlock *> test_set(test_blocks.begin(),
                                                  test_blocks.end());
    std::unordered_set<ir::BasicBlock *> targets;
    bool unsafe = false;
    for (const auto &c : cases) {
      if (test_set.count(c.second) || !targets.insert(c.second).second) {
        unsafe = true;
        break;
      }
    }
    if (unsafe || test_set.count(default_block) ||
        !targets.insert(default_block).second) {
      continue;
    }

    // ---- Apply ----
    // Replace the head's comparison+branch with the switch. Erase the branch
    // first (it uses the icmp), then the now-dead icmp.
    head->erase_instruction(head_match.branch);
    head->erase_instruction(head_match.icmp);
    head->append<ir::SwitchInst>(head_match.scrutinee_sp, default_block,
                                 std::move(cases));

    // Redirect phi incomings from the removed predecessors to the head.
    for (const auto &[target, old_pred] : phi_fixups) {
      if (old_pred == head) {
        continue; // arm_0 / single-case default already come from head
      }
      for (const auto &inst : target->instructions()) {
        auto *phi = dynamic_cast<ir::PhiInst *>(inst.get());
        if (!phi) {
          break; // phis are grouped at the block start
        }
        phi->replace_incoming_block(old_pred, head);
      }
    }

    // Erase the folded test blocks (now unreachable).
    std::unordered_set<ir::BasicBlock *> to_remove(remove_blocks.begin(),
                                                   remove_blocks.end());
    std::vector<std::shared_ptr<ir::BasicBlock>> removal_handles;
    for (const auto &b : function.blocks()) {
      if (to_remove.count(b.get())) {
        removal_handles.push_back(b);
      }
    }
    for (const auto &b : removal_handles) {
      function.erase_block(b);
    }

    return true; // mutated; caller restarts the scan
  }

  return false;
}

} // namespace rc::opt
