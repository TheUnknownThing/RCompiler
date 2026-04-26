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
  void run(const std::vector<std::unique_ptr<AsmFunction>> &functions) const;

private:
  static constexpr std::array<int, 11> kCalleeSavedRegs = {
      9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

  size_t alignTo(size_t value, size_t align) const;
  bool isPhysicalRegister(const std::shared_ptr<AsmOperand> &operand,
                          int regId) const;
  bool hasCall(const AsmFunction &function) const;
  std::vector<int> usedCalleeSavedRegisters(const AsmFunction &function) const;
  std::shared_ptr<Register> physical(size_t id) const;
  std::shared_ptr<Immediate> immediate(int32_t value) const;
  std::shared_ptr<StackSlot> stackSlot(size_t offset, size_t size) const;
  std::vector<std::unique_ptr<AsmInst>> adjustSp(int32_t delta) const;
};










} // namespace rc::backend
