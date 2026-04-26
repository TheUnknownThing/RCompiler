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
    size_t vregId{0};
    size_t start{std::numeric_limits<size_t>::max()};
    size_t end{0};
    int assignedPhysReg{-1};
    bool spilled{false};
    bool crossesCall{false};
    std::shared_ptr<StackSlot> spillSlot;
  };

  struct BlockLiveness {
    std::unordered_set<size_t> use;
    std::unordered_set<size_t> def;
    std::unordered_set<size_t> liveIn;
    std::unordered_set<size_t> liveOut;
    size_t startPos{0};
    size_t endPos{0};
  };

  struct FixedRegisterSpan {
    size_t start{std::numeric_limits<size_t>::max()};
    size_t end{0};
    bool valid{false};
  };

  static constexpr size_t kSpillSlotSize = 4;
  static constexpr std::array<int, 14> kCallerSavedRegs = {
      6,  7,  10, 11, 12, 13, 14, 15, 16, 17, 28, 29, 30, 31};
  static constexpr std::array<int, 11> kCalleeSavedRegs = {
      9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

  size_t alignTo(size_t value, size_t align) const;
  size_t maxVirtualRegisterId(const AsmFunction &function) const;
  std::vector<std::vector<size_t>>
  computeSuccessors(const AsmFunction &function) const;
  std::unordered_map<int, FixedRegisterSpan>
  computeFixedRegisterSpans(const AsmFunction &function) const;
  std::vector<LiveInterval> buildIntervals(const AsmFunction &function) const;
  void linearScan(AsmFunction &function,
                  std::vector<LiveInterval> &intervals) const;
  void graphColor(AsmFunction &function,
                  std::vector<LiveInterval> &intervals) const;
  bool rewriteSpills(AsmFunction &function,
                     const std::vector<LiveInterval> &intervals,
                     size_t &nextVirtualRegId) const;
  void
  assignPhysicalRegisters(AsmFunction &function,
                          const std::vector<LiveInterval> &intervals) const;
  std::shared_ptr<Register> createVirtualRegister(size_t id) const;
  std::shared_ptr<Register> createPhysicalRegister(size_t id) const;
  std::shared_ptr<StackSlot> createSpillSlot(AsmFunction &function) const;
  void clearSpillFlags(AsmFunction &function) const;
  void markSpilledRegisters(AsmFunction &function,
                            const std::unordered_set<size_t> &spilled) const;
  bool isVirtualRegisterOperand(const std::shared_ptr<AsmOperand> &operand,
                                size_t *id = nullptr) const;
  bool isPhysicalRegisterOperand(const std::shared_ptr<AsmOperand> &operand,
                                 int *id = nullptr) const;
  bool isCall(const AsmInst &inst) const;
  bool isTerminator(const AsmInst &inst) const;
  const std::vector<int> &candidateRegisters(bool crossesCall) const;
  bool overlapsFixedRegister(const FixedRegisterSpan &span, size_t start,
                             size_t end) const;
  size_t blockLabelIndex(const std::vector<std::shared_ptr<AsmOperand>> &uses,
                         InstOpcode opcode) const;
  void extendInterval(std::unordered_map<size_t, LiveInterval> &intervals,
                      size_t vregId, size_t start, size_t end) const;
  std::unique_ptr<AsmInst>
  cloneInst(const AsmInst &inst, const std::shared_ptr<AsmOperand> &dst,
            const std::vector<std::shared_ptr<AsmOperand>> &uses) const;
};

























} // namespace rc::backend
