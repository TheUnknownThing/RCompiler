#include "backend/passes/prologueEpilogue.hpp"

namespace rc::backend {

void PrologueEpiloguePass::run(
    const std::vector<std::unique_ptr<AsmFunction>> &functions) const {
  for (const auto &function : functions) {
    if (!function || function->blocks.empty()) {
      continue;
    }

    auto usedCalleeSaved = usedCalleeSavedRegisters(*function);
    bool savesRa = hasCall(*function);

    size_t saveAreaOffset = function->stackSize;
    std::vector<std::pair<int, std::shared_ptr<StackSlot>>> savedRegs;
    if (savesRa) {
      auto slot = stackSlot(saveAreaOffset, 4);
      saveAreaOffset += 4;
      savedRegs.emplace_back(1, slot);
    }
    for (int regId : usedCalleeSaved) {
      auto slot = stackSlot(saveAreaOffset, 4);
      saveAreaOffset += 4;
      savedRegs.emplace_back(regId, slot);
    }

    function->stackSize = alignTo(saveAreaOffset, 16);
    if (function->stackSize == 0 && savedRegs.empty()) {
      continue;
    }

    auto &entry = function->blocks.front();
    std::vector<std::unique_ptr<AsmInst>> prologue;
    if (function->stackSize != 0) {
      auto adjust = adjustSp(-static_cast<int32_t>(function->stackSize));
      for (auto &inst : adjust) {
        prologue.push_back(std::move(inst));
      }
    }
    for (const auto &[regId, slot] : savedRegs) {
      prologue.push_back(std::make_unique<AsmInst>(
          InstOpcode::SW,
          std::vector<std::shared_ptr<AsmOperand>>{physical(regId), slot}));
    }

    std::vector<std::unique_ptr<AsmInst>> newEntry;
    newEntry.reserve(prologue.size() + entry->instructions.size());
    for (auto &inst : prologue) {
      newEntry.push_back(std::move(inst));
    }
    for (auto &inst : entry->instructions) {
      newEntry.push_back(std::move(inst));
    }
    entry->instructions = std::move(newEntry);

    for (auto &block : function->blocks) {
      if (!block) {
        continue;
      }

      std::vector<std::unique_ptr<AsmInst>> rewritten;
      for (auto &inst : block->instructions) {
        if (!inst) {
          continue;
        }

        if (inst->getOpcode() != InstOpcode::RET) {
          rewritten.push_back(std::move(inst));
          continue;
        }

        for (auto it = savedRegs.rbegin(); it != savedRegs.rend(); ++it) {
          rewritten.push_back(std::make_unique<AsmInst>(
              InstOpcode::LW, physical(static_cast<size_t>(it->first)),
              std::vector<std::shared_ptr<AsmOperand>>{it->second}));
        }
        if (function->stackSize != 0) {
          auto adjust = adjustSp(static_cast<int32_t>(function->stackSize));
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
size_t PrologueEpiloguePass::alignTo(size_t value, size_t align) const {
  if (align <= 1) {
    return value;
  }
  auto rem = value % align;
  return rem == 0 ? value : value + (align - rem);
}
bool PrologueEpiloguePass::isPhysicalRegister(
    const std::shared_ptr<AsmOperand> &operand, int regId) const {
  auto reg = std::dynamic_pointer_cast<Register>(operand);
  return reg && !reg->is_virtual && static_cast<int>(reg->id) == regId;
}
bool PrologueEpiloguePass::hasCall(const AsmFunction &function) const {
  for (const auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (const auto &inst : block->instructions) {
      if (inst && inst->getOpcode() == InstOpcode::CALL) {
        return true;
      }
    }
  }
  return false;
}
std::vector<int> PrologueEpiloguePass::usedCalleeSavedRegisters(
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

      for (int regId : kCalleeSavedRegs) {
        if (isPhysicalRegister(inst->getDst(), regId)) {
          used.insert(regId);
        }
        for (const auto &use : inst->getUses()) {
          if (isPhysicalRegister(use, regId)) {
            used.insert(regId);
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
PrologueEpiloguePass::stackSlot(size_t offset, size_t size) const {
  return std::make_shared<StackSlot>(offset, size);
}
std::vector<std::unique_ptr<AsmInst>>
PrologueEpiloguePass::adjustSp(int32_t delta) const {
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
