#pragma once

#include "backend/nodes/instructions.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <vector>

namespace rc::backend {

class PrologueEpiloguePass {
public:
  PrologueEpiloguePass() = default;
  PrologueEpiloguePass(size_t saved_reg_size, InstOpcode save_opcode,
                       InstOpcode restore_opcode)
      : saved_reg_size_(saved_reg_size), save_opcode_(save_opcode),
        restore_opcode_(restore_opcode) {}

  void run(const std::vector<std::unique_ptr<AsmFunction>> &functions) const;

private:
  static constexpr std::array<int, 11> k_callee_saved_regs = {
      9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
  size_t saved_reg_size_{4};
  InstOpcode save_opcode_{InstOpcode::SW};
  InstOpcode restore_opcode_{InstOpcode::LW};

  size_t align_to(size_t value, size_t align) const;
  bool is_physical_register(const std::shared_ptr<AsmOperand> &operand,
                          int reg_id) const;
  bool has_call(const AsmFunction &function) const;
  std::vector<int> used_callee_saved_registers(const AsmFunction &function) const;
  std::shared_ptr<Register> physical(size_t id) const;
  std::shared_ptr<Immediate> immediate(int32_t value) const;
  std::shared_ptr<StackSlot> stack_slot(size_t offset, size_t size) const;
  std::vector<std::unique_ptr<AsmInst>> adjust_sp(int32_t delta) const;
};

} // namespace rc::backend
