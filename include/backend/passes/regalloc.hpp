#pragma once

#include "backend/nodes/instructions.hpp"
#include "backend/nodes/operands.hpp"

#include "utils/logger.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rc::backend {

class RegAlloc {
public:
  void allocate(const std::vector<std::unique_ptr<AsmFunction>> &functions);

private:
  struct LiveInterval {
    size_t vreg_id{0};
    size_t start{std::numeric_limits<size_t>::max()};
    size_t end{0};
    int assigned_phys_reg{-1};
    bool spilled{false};
    bool crosses_call{false};
    std::shared_ptr<StackSlot> spill_slot;
  };

  struct BlockLiveness {
    std::unordered_set<size_t> use;
    std::unordered_set<size_t> def;
    std::unordered_set<size_t> live_in;
    std::unordered_set<size_t> live_out;
    size_t start_pos{0};
    size_t end_pos{0};
  };

  struct FixedRegisterSpan {
    size_t start{std::numeric_limits<size_t>::max()};
    size_t end{0};
    bool valid{false};
  };

  static constexpr size_t k_spill_slot_size = 4;
  static constexpr std::array<int, 14> k_caller_saved_regs = {
      6,  7,  10, 11, 12, 13, 14, 15, 16, 17, 28, 29, 30, 31};
  static constexpr std::array<int, 11> k_callee_saved_regs = {
      9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

  size_t align_to(size_t value, size_t align) const;
  size_t max_virtual_register_id(const AsmFunction &function) const;
  std::vector<std::vector<size_t>>
  compute_successors(const AsmFunction &function) const;
  std::unordered_map<int, FixedRegisterSpan>
  compute_fixed_register_spans(const AsmFunction &function) const;
  std::vector<LiveInterval> build_intervals(const AsmFunction &function) const;
  void linear_scan(AsmFunction &function,
                  std::vector<LiveInterval> &intervals) const;
  void graph_color(AsmFunction &function,
                  std::vector<LiveInterval> &intervals) const;
  bool rewrite_spills(AsmFunction &function,
                     const std::vector<LiveInterval> &intervals,
                     size_t &next_virtual_reg_id) const;
  void
  assign_physical_registers(AsmFunction &function,
                          const std::vector<LiveInterval> &intervals) const;
  std::shared_ptr<Register> create_virtual_register(size_t id) const;
  std::shared_ptr<Register> create_physical_register(size_t id) const;
  std::shared_ptr<StackSlot> create_spill_slot(AsmFunction &function) const;
  void clear_spill_flags(AsmFunction &function) const;
  void mark_spilled_registers(AsmFunction &function,
                            const std::unordered_set<size_t> &spilled) const;
  bool is_virtual_register_operand(const std::shared_ptr<AsmOperand> &operand,
                                size_t *id = nullptr) const;
  bool is_physical_register_operand(const std::shared_ptr<AsmOperand> &operand,
                                 int *id = nullptr) const;
  bool is_call(const AsmInst &inst) const;
  bool is_terminator(const AsmInst &inst) const;
  const std::vector<int> &candidate_registers(bool crosses_call) const;
  bool overlaps_fixed_register(const FixedRegisterSpan &span, size_t start,
                             size_t end) const;
  size_t block_label_index(const std::vector<std::shared_ptr<AsmOperand>> &uses,
                         InstOpcode opcode) const;
  void extend_interval(std::unordered_map<size_t, LiveInterval> &intervals,
                      size_t vreg_id, size_t start, size_t end) const;
  std::unique_ptr<AsmInst>
  clone_inst(const AsmInst &inst, const std::shared_ptr<AsmOperand> &dst,
            const std::vector<std::shared_ptr<AsmOperand>> &uses) const;
};

} // namespace rc::backend
