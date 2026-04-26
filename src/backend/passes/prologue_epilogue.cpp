#include "backend/passes/prologue_epilogue.hpp"

namespace rc::backend {

void PrologueEpiloguePass::run(
    const std::vector<std::unique_ptr<AsmFunction>> &functions) const {
  for (const auto &function : functions) {
    if (!function || function->blocks.empty()) {
      continue;
    }

    auto used_callee_saved = used_callee_saved_registers(*function);
    bool saves_ra = has_call(*function);

    size_t save_area_offset = function->stack_size;
    std::vector<std::pair<int, std::shared_ptr<StackSlot>>> saved_regs;
    if (saves_ra) {
      auto slot = stack_slot(save_area_offset, 4);
      save_area_offset += 4;
      saved_regs.emplace_back(1, slot);
    }
    for (int reg_id : used_callee_saved) {
      auto slot = stack_slot(save_area_offset, 4);
      save_area_offset += 4;
      saved_regs.emplace_back(reg_id, slot);
    }

    function->stack_size = align_to(save_area_offset, 16);
    if (function->stack_size == 0 && saved_regs.empty()) {
      continue;
    }

    auto &entry = function->blocks.front();
    std::vector<std::unique_ptr<AsmInst>> prologue;
    if (function->stack_size != 0) {
      auto adjust = adjust_sp(-static_cast<int32_t>(function->stack_size));
      for (auto &inst : adjust) {
        prologue.push_back(std::move(inst));
      }
    }
    for (const auto &[reg_id, slot] : saved_regs) {
      prologue.push_back(std::make_unique<AsmInst>(
          InstOpcode::SW,
          std::vector<std::shared_ptr<AsmOperand>>{physical(reg_id), slot}));
    }

    std::vector<std::unique_ptr<AsmInst>> new_entry;
    new_entry.reserve(prologue.size() + entry->instructions.size());
    for (auto &inst : prologue) {
      new_entry.push_back(std::move(inst));
    }
    for (auto &inst : entry->instructions) {
      new_entry.push_back(std::move(inst));
    }
    entry->instructions = std::move(new_entry);

    for (auto &block : function->blocks) {
      if (!block) {
        continue;
      }

      std::vector<std::unique_ptr<AsmInst>> rewritten;
      for (auto &inst : block->instructions) {
        if (!inst) {
          continue;
        }

        if (inst->get_opcode() != InstOpcode::RET) {
          rewritten.push_back(std::move(inst));
          continue;
        }

        for (auto it = saved_regs.rbegin(); it != saved_regs.rend(); ++it) {
          rewritten.push_back(std::make_unique<AsmInst>(
              InstOpcode::LW, physical(static_cast<size_t>(it->first)),
              std::vector<std::shared_ptr<AsmOperand>>{it->second}));
        }
        if (function->stack_size != 0) {
          auto adjust = adjust_sp(static_cast<int32_t>(function->stack_size));
          for (auto &adj : adjust) {
            rewritten.push_back(std::move(adj));
          }
        }
        rewritten.push_back(std::move(inst));
      }

      block->instructions = std::move(rewritten);
    }
  }
}
size_t PrologueEpiloguePass::align_to(size_t value, size_t align) const {
  if (align <= 1) {
    return value;
  }
  auto rem = value % align;
  return rem == 0 ? value : value + (align - rem);
}
bool PrologueEpiloguePass::is_physical_register(
    const std::shared_ptr<AsmOperand> &operand, int reg_id) const {
  auto reg = std::dynamic_pointer_cast<Register>(operand);
  return reg && !reg->is_virtual && static_cast<int>(reg->id) == reg_id;
}
bool PrologueEpiloguePass::has_call(const AsmFunction &function) const {
  for (const auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (const auto &inst : block->instructions) {
      if (inst && inst->get_opcode() == InstOpcode::CALL) {
        return true;
      }
    }
  }
  return false;
}
std::vector<int> PrologueEpiloguePass::used_callee_saved_registers(
    const AsmFunction &function) const {
  std::unordered_set<int> used;
  for (const auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      for (int reg_id : k_callee_saved_regs) {
        if (is_physical_register(inst->get_dst(), reg_id)) {
          used.insert(reg_id);
        }
        for (const auto &use : inst->get_uses()) {
          if (is_physical_register(use, reg_id)) {
            used.insert(reg_id);
          }
        }
      }
    }
  }

  std::vector<int> regs(used.begin(), used.end());
  std::sort(regs.begin(), regs.end());
  return regs;
}
std::shared_ptr<Register>
PrologueEpiloguePass::physical(size_t id) const {
  auto reg = std::make_shared<Register>();
  reg->id = id;
  reg->is_virtual = false;
  reg->spilled = false;
  return reg;
}
std::shared_ptr<Immediate>
PrologueEpiloguePass::immediate(int32_t value) const {
  return std::make_shared<Immediate>(value);
}
std::shared_ptr<StackSlot>
PrologueEpiloguePass::stack_slot(size_t offset, size_t size) const {
  return std::make_shared<StackSlot>(offset, size);
}
std::vector<std::unique_ptr<AsmInst>>
PrologueEpiloguePass::adjust_sp(int32_t delta) const {
  std::vector<std::unique_ptr<AsmInst>> insts;
  auto sp = physical(2);

  if (delta >= -2048 && delta <= 2047) {
    insts.push_back(std::make_unique<AsmInst>(
        InstOpcode::ADDI, sp,
        std::vector<std::shared_ptr<AsmOperand>>{sp, immediate(delta)}));
    return insts;
  }

  auto scratch = physical(5);
  insts.push_back(std::make_unique<AsmInst>(
      InstOpcode::LI, scratch,
      std::vector<std::shared_ptr<AsmOperand>>{immediate(delta)}));
  insts.push_back(std::make_unique<AsmInst>(
      InstOpcode::ADD, sp,
      std::vector<std::shared_ptr<AsmOperand>>{sp, scratch}));
  return insts;
}

} // namespace rc::backend
