#pragma once

#include "backend/nodes/instructions.hpp"
#include "backend/nodes/operands.hpp"

#include "utils/logger.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
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
  static constexpr std::array<int, 15> kCallerSavedRegs = {
      5,  6,  7,  10, 11, 12, 13, 14, 15, 16, 17, 28, 29, 30, 31};
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

inline void
RegAlloc::allocate(const std::vector<std::unique_ptr<AsmFunction>> &functions) {
  for (const auto &function : functions) {
    if (!function) {
      continue;
    }

    LOG_DEBUG("[RegAlloc] Begin function: " + function->name);
    size_t nextVirtualRegId = maxVirtualRegisterId(*function) + 1;
    constexpr size_t kMaxRewriteIterations = 32;

    bool finished = false;
    for (size_t iteration = 0; iteration < kMaxRewriteIterations; ++iteration) {
      clearSpillFlags(*function);

      auto intervals = buildIntervals(*function);
      if (intervals.empty()) {
        finished = true;
        break;
      }

      linearScan(*function, intervals);

      bool hasSpills = false;
      for (const auto &interval : intervals) {
        if (interval.spilled) {
          hasSpills = true;
          break;
        }
      }

      if (!hasSpills) {
        assignPhysicalRegisters(*function, intervals);
        finished = true;
        break;
      }

      if (!rewriteSpills(*function, intervals, nextVirtualRegId)) {
        throw std::runtime_error("RegAlloc: spill rewrite failed to make "
                                 "progress for function " +
                                 function->name);
      }
    }

    if (!finished) {
      throw std::runtime_error("RegAlloc: failed to allocate registers for " +
                               function->name);
    }

    function->stackSize = alignTo(function->stackSize, 16);
    LOG_DEBUG("[RegAlloc] End function: " + function->name);
  }
}

inline size_t RegAlloc::alignTo(size_t value, size_t align) const {
  if (align <= 1) {
    return value;
  }
  auto rem = value % align;
  return rem == 0 ? value : (value + (align - rem));
}

inline size_t
RegAlloc::maxVirtualRegisterId(const AsmFunction &function) const {
  size_t maxId = 0;
  for (const auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      size_t id = 0;
      if (isVirtualRegisterOperand(inst->getDst(), &id)) {
        maxId = std::max(maxId, id);
      }
      for (const auto &use : inst->getUses()) {
        if (isVirtualRegisterOperand(use, &id)) {
          maxId = std::max(maxId, id);
        }
      }
    }
  }
  return maxId;
}

inline std::vector<std::vector<size_t>>
RegAlloc::computeSuccessors(const AsmFunction &function) const {
  std::unordered_map<std::string, size_t> blockIndexByName;
  for (size_t i = 0; i < function.blocks.size(); ++i) {
    if (function.blocks[i]) {
      blockIndexByName[function.blocks[i]->name] = i;
    }
  }

  std::vector<std::vector<size_t>> successors(function.blocks.size());
  for (size_t i = 0; i < function.blocks.size(); ++i) {
    const auto &block = function.blocks[i];
    if (!block || block->instructions.empty()) {
      if (i + 1 < function.blocks.size()) {
        successors[i].push_back(i + 1);
      }
      continue;
    }

    const auto &terminator = block->instructions.back();
    if (!terminator) {
      continue;
    }

    const auto opcode = terminator->getOpcode();
    const auto &uses = terminator->getUses();

    if (opcode == InstOpcode::J) {
      size_t labelPos = blockLabelIndex(uses, opcode);
      if (labelPos < uses.size()) {
        auto label = std::dynamic_pointer_cast<Symbol>(uses[labelPos]);
        auto it =
            label ? blockIndexByName.find(label->name) : blockIndexByName.end();
        if (it != blockIndexByName.end()) {
          successors[i].push_back(it->second);
        }
      }
      continue;
    }

    if (opcode == InstOpcode::BNEZ || opcode == InstOpcode::BEQZ ||
        opcode == InstOpcode::BEQ || opcode == InstOpcode::BNE ||
        opcode == InstOpcode::BLT || opcode == InstOpcode::BGE ||
        opcode == InstOpcode::BLTU || opcode == InstOpcode::BGEU) {
      size_t firstLabel = blockLabelIndex(uses, opcode);
      for (size_t pos = firstLabel; pos < uses.size(); ++pos) {
        auto label = std::dynamic_pointer_cast<Symbol>(uses[pos]);
        auto it =
            label ? blockIndexByName.find(label->name) : blockIndexByName.end();
        if (it != blockIndexByName.end()) {
          successors[i].push_back(it->second);
        }
      }
      continue;
    }

    if (!isTerminator(*terminator) && i + 1 < function.blocks.size()) {
      successors[i].push_back(i + 1);
    }
  }

  return successors;
}

inline std::unordered_map<int, RegAlloc::FixedRegisterSpan>
RegAlloc::computeFixedRegisterSpans(const AsmFunction &function) const {
  std::unordered_map<int, FixedRegisterSpan> spans;
  size_t position = 0;

  for (const auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      auto record = [&](const std::shared_ptr<AsmOperand> &operand) {
        int regId = -1;
        if (!isPhysicalRegisterOperand(operand, &regId)) {
          return;
        }

        auto &span = spans[regId];
        if (!span.valid) {
          span.start = position;
          span.end = position;
          span.valid = true;
          return;
        }

        span.start = std::min(span.start, position);
        span.end = std::max(span.end, position);
      };

      record(inst->getDst());
      for (const auto &use : inst->getUses()) {
        record(use);
      }
      ++position;
    }
  }

  return spans;
}

inline std::vector<RegAlloc::LiveInterval>
RegAlloc::buildIntervals(const AsmFunction &function) const {
  std::vector<BlockLiveness> blocks(function.blocks.size());
  size_t nextPosition = 0;

  for (size_t blockIndex = 0; blockIndex < function.blocks.size();
       ++blockIndex) {
    const auto &block = function.blocks[blockIndex];
    auto &info = blocks[blockIndex];
    info.startPos = nextPosition;

    if (!block || block->instructions.empty()) {
      info.endPos = nextPosition;
      continue;
    }

    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      size_t regId = 0;
      for (const auto &use : inst->getUses()) {
        if (!isVirtualRegisterOperand(use, &regId)) {
          continue;
        }
        if (info.def.find(regId) == info.def.end()) {
          info.use.insert(regId);
        }
      }

      if (isVirtualRegisterOperand(inst->getDst(), &regId)) {
        info.def.insert(regId);
      }

      ++nextPosition;
    }

    info.endPos =
        nextPosition == info.startPos ? info.startPos : nextPosition - 1;
  }

  const auto successors = computeSuccessors(function);
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = function.blocks.size(); i-- > 0;) {
      std::unordered_set<size_t> newLiveOut;
      for (size_t succ : successors[i]) {
        newLiveOut.insert(blocks[succ].liveIn.begin(),
                          blocks[succ].liveIn.end());
      }

      std::unordered_set<size_t> newLiveIn = blocks[i].use;
      for (size_t regId : newLiveOut) {
        if (blocks[i].def.find(regId) == blocks[i].def.end()) {
          newLiveIn.insert(regId);
        }
      }

      if (newLiveOut != blocks[i].liveOut || newLiveIn != blocks[i].liveIn) {
        blocks[i].liveOut = std::move(newLiveOut);
        blocks[i].liveIn = std::move(newLiveIn);
        changed = true;
      }
    }
  }

  std::unordered_map<size_t, LiveInterval> intervalMap;
  std::vector<size_t> callPositions;
  size_t position = 0;

  for (size_t blockIndex = 0; blockIndex < function.blocks.size();
       ++blockIndex) {
    const auto &block = function.blocks[blockIndex];
    const auto &info = blocks[blockIndex];

    if (!block || block->instructions.empty()) {
      continue;
    }

    for (size_t regId : info.liveIn) {
      extendInterval(intervalMap, regId, info.startPos, info.endPos);
    }
    for (size_t regId : info.liveOut) {
      extendInterval(intervalMap, regId, info.startPos, info.endPos);
    }

    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      if (isCall(*inst)) {
        callPositions.push_back(position);
      }

      size_t regId = 0;
      for (const auto &use : inst->getUses()) {
        if (isVirtualRegisterOperand(use, &regId)) {
          extendInterval(intervalMap, regId, position, position);
        }
      }
      if (isVirtualRegisterOperand(inst->getDst(), &regId)) {
        extendInterval(intervalMap, regId, position, position);
      }

      ++position;
    }
  }

  std::vector<LiveInterval> intervals;
  intervals.reserve(intervalMap.size());
  for (auto &[_, interval] : intervalMap) {
    for (size_t callPos : callPositions) {
      if (interval.start < callPos && callPos < interval.end) {
        interval.crossesCall = true;
        break;
      }
    }
    intervals.push_back(interval);
  }

  std::sort(intervals.begin(), intervals.end(),
            [](const LiveInterval &lhs, const LiveInterval &rhs) {
              if (lhs.start != rhs.start) {
                return lhs.start < rhs.start;
              }
              if (lhs.end != rhs.end) {
                return lhs.end < rhs.end;
              }
              return lhs.vregId < rhs.vregId;
            });

  return intervals;
}

inline void RegAlloc::linearScan(AsmFunction &function,
                                 std::vector<LiveInterval> &intervals) const {
  std::unordered_set<size_t> spilledIds;
  std::vector<LiveInterval *> active;
  const auto fixedRegSpans = computeFixedRegisterSpans(function);
  auto sortActive = [&]() {
    std::sort(active.begin(), active.end(),
              [](const LiveInterval *lhs, const LiveInterval *rhs) {
                if (lhs->end != rhs->end) {
                  return lhs->end < rhs->end;
                }
                return lhs->vregId < rhs->vregId;
              });
  };

  auto allocateSpill = [&](LiveInterval &interval) {
    interval.spilled = true;
    interval.assignedPhysReg = -1;
    interval.spillSlot = createSpillSlot(function);
    spilledIds.insert(interval.vregId);
  };

  for (auto &interval : intervals) {
    active.erase(std::remove_if(active.begin(), active.end(),
                                [&](const LiveInterval *other) {
                                  return other->end < interval.start;
                                }),
                 active.end());
    sortActive();

    const auto &candidates = candidateRegisters(interval.crossesCall);
    std::vector<int> available;
    available.reserve(candidates.size());

    for (int physReg : candidates) {
      bool occupied = false;
      for (const auto *live : active) {
        if (live->assignedPhysReg == physReg) {
          occupied = true;
          break;
        }
      }
      if (occupied) {
        continue;
      }

      auto fixedIt = fixedRegSpans.find(physReg);
      if (fixedIt != fixedRegSpans.end() &&
          overlapsFixedRegister(fixedIt->second, interval.start, interval.end)) {
        continue;
      }

      available.push_back(physReg);
    }

    if (available.empty()) {
      LiveInterval *spill = nullptr;
      auto spillIt = active.end();
      for (auto it = active.begin(); it != active.end(); ++it) {
        if (std::find(candidates.begin(), candidates.end(),
                      (*it)->assignedPhysReg) == candidates.end()) {
          continue;
        }

        if (!spill || spill->end < (*it)->end ||
            (spill->end == (*it)->end && spill->vregId < (*it)->vregId)) {
          spill = *it;
          spillIt = it;
        }
      }

      if (!spill || spillIt == active.end()) {
        allocateSpill(interval);
        continue;
      }

      if (spill->end > interval.end) {
        interval.assignedPhysReg = spill->assignedPhysReg;
        spill->assignedPhysReg = -1;
        spill->spillSlot = createSpillSlot(function);
        spill->spilled = true;
        spilledIds.insert(spill->vregId);
        *spillIt = &interval;
        sortActive();
      } else {
        allocateSpill(interval);
      }
      continue;
    }

    interval.assignedPhysReg = available.front();
    active.push_back(&interval);
    sortActive();
  }

  markSpilledRegisters(function, spilledIds);
}

inline bool RegAlloc::rewriteSpills(AsmFunction &function,
                                    const std::vector<LiveInterval> &intervals,
                                    size_t &nextVirtualRegId) const {
  std::unordered_map<size_t, std::shared_ptr<StackSlot>> spilledSlots;
  for (const auto &interval : intervals) {
    if (interval.spilled && interval.spillSlot) {
      spilledSlots[interval.vregId] = interval.spillSlot;
    }
  }

  if (spilledSlots.empty()) {
    return false;
  }

  bool changed = false;
  for (auto &block : function.blocks) {
    if (!block) {
      continue;
    }

    std::vector<std::unique_ptr<AsmInst>> rewritten;
    rewritten.reserve(block->instructions.size() * 3);

    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      std::vector<std::shared_ptr<AsmOperand>> newUses = inst->getUses();
      std::vector<std::unique_ptr<AsmInst>> before;
      std::vector<std::unique_ptr<AsmInst>> after;
      std::unordered_map<size_t, std::shared_ptr<Register>> loadedTemps;

      auto loadSpilledUse = [&](size_t regId,
                                const std::shared_ptr<StackSlot> &slot)
          -> std::shared_ptr<Register> {
        auto found = loadedTemps.find(regId);
        if (found != loadedTemps.end()) {
          return found->second;
        }

        auto temp = createVirtualRegister(nextVirtualRegId++);
        before.push_back(std::make_unique<AsmInst>(
            InstOpcode::LW, temp,
            std::vector<std::shared_ptr<AsmOperand>>{slot}));
        loadedTemps.emplace(regId, temp);
        return temp;
      };

      for (size_t i = 0; i < newUses.size(); ++i) {
        size_t regId = 0;
        if (!isVirtualRegisterOperand(newUses[i], &regId)) {
          continue;
        }

        auto spillIt = spilledSlots.find(regId);
        if (spillIt == spilledSlots.end()) {
          continue;
        }

        newUses[i] = loadSpilledUse(regId, spillIt->second);
        changed = true;
      }

      auto newDst = inst->getDst();
      size_t dstId = 0;
      if (isVirtualRegisterOperand(newDst, &dstId)) {
        auto spillIt = spilledSlots.find(dstId);
        if (spillIt != spilledSlots.end()) {
          auto tempDst = createVirtualRegister(nextVirtualRegId++);
          newDst = tempDst;
          after.push_back(std::make_unique<AsmInst>(
              InstOpcode::SW, std::vector<std::shared_ptr<AsmOperand>>{
                                  tempDst, spillIt->second}));
          changed = true;
        }
      }

      for (auto &pre : before) {
        rewritten.push_back(std::move(pre));
      }
      rewritten.push_back(cloneInst(*inst, newDst, newUses));
      for (auto &post : after) {
        rewritten.push_back(std::move(post));
      }
    }

    block->instructions = std::move(rewritten);
  }

  return changed;
}

inline void RegAlloc::assignPhysicalRegisters(
    AsmFunction &function, const std::vector<LiveInterval> &intervals) const {
  std::unordered_map<size_t, int> assignment;
  for (const auto &interval : intervals) {
    if (interval.spilled || interval.assignedPhysReg < 0) {
      throw std::runtime_error("RegAlloc: virtual register left without a "
                               "physical assignment");
    }
    assignment[interval.vregId] = interval.assignedPhysReg;
  }

  std::unordered_map<int, std::shared_ptr<Register>> physicalRegs;
  auto materialize = [&](const std::shared_ptr<AsmOperand> &operand)
      -> std::shared_ptr<AsmOperand> {
    size_t regId = 0;
    if (!isVirtualRegisterOperand(operand, &regId)) {
      return operand;
    }

    auto it = assignment.find(regId);
    if (it == assignment.end()) {
      throw std::runtime_error("RegAlloc: missing assignment for v" +
                               std::to_string(regId));
    }

    auto cacheIt = physicalRegs.find(it->second);
    if (cacheIt != physicalRegs.end()) {
      return cacheIt->second;
    }

    auto reg = createPhysicalRegister(static_cast<size_t>(it->second));
    physicalRegs.emplace(it->second, reg);
    return reg;
  };

  for (auto &block : function.blocks) {
    if (!block) {
      continue;
    }

    std::vector<std::unique_ptr<AsmInst>> rewritten;
    rewritten.reserve(block->instructions.size());
    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      std::vector<std::shared_ptr<AsmOperand>> uses;
      uses.reserve(inst->getUses().size());
      for (const auto &use : inst->getUses()) {
        uses.push_back(materialize(use));
      }
      rewritten.push_back(cloneInst(*inst, materialize(inst->getDst()), uses));
    }
    block->instructions = std::move(rewritten);
  }
}

inline std::shared_ptr<Register>
RegAlloc::createVirtualRegister(size_t id) const {
  auto reg = std::make_shared<Register>();
  reg->id = id;
  reg->is_virtual = true;
  reg->spilled = false;
  return reg;
}

inline std::shared_ptr<Register>
RegAlloc::createPhysicalRegister(size_t id) const {
  auto reg = std::make_shared<Register>();
  reg->id = id;
  reg->is_virtual = false;
  reg->spilled = false;
  return reg;
}

inline std::shared_ptr<StackSlot>
RegAlloc::createSpillSlot(AsmFunction &function) const {
  size_t offset = alignTo(function.stackSize, kSpillSlotSize);
  auto slot = std::make_shared<StackSlot>(offset, kSpillSlotSize);
  function.stackSize = offset + kSpillSlotSize;
  return slot;
}

inline void RegAlloc::clearSpillFlags(AsmFunction &function) const {
  for (auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      if (auto dst = std::dynamic_pointer_cast<Register>(inst->getDst());
          dst && dst->is_virtual) {
        dst->spilled = false;
      }

      for (const auto &use : inst->getUses()) {
        auto reg = std::dynamic_pointer_cast<Register>(use);
        if (reg && reg->is_virtual) {
          reg->spilled = false;
        }
      }
    }
  }
}

inline void RegAlloc::markSpilledRegisters(
    AsmFunction &function, const std::unordered_set<size_t> &spilled) const {
  if (spilled.empty()) {
    return;
  }

  auto mark = [&](const std::shared_ptr<AsmOperand> &operand) {
    auto reg = std::dynamic_pointer_cast<Register>(operand);
    if (reg && reg->is_virtual && spilled.find(reg->id) != spilled.end()) {
      reg->spilled = true;
    }
  };

  for (auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }
      mark(inst->getDst());
      for (const auto &use : inst->getUses()) {
        mark(use);
      }
    }
  }
}

inline bool
RegAlloc::isVirtualRegisterOperand(const std::shared_ptr<AsmOperand> &operand,
                                   size_t *id) const {
  auto reg = std::dynamic_pointer_cast<Register>(operand);
  if (!reg || !reg->is_virtual) {
    return false;
  }
  if (id) {
    *id = reg->id;
  }
  return true;
}

inline bool
RegAlloc::isPhysicalRegisterOperand(const std::shared_ptr<AsmOperand> &operand,
                                    int *id) const {
  auto reg = std::dynamic_pointer_cast<Register>(operand);
  if (!reg || reg->is_virtual) {
    return false;
  }
  if (id) {
    *id = static_cast<int>(reg->id);
  }
  return true;
}

inline bool RegAlloc::isCall(const AsmInst &inst) const {
  return inst.getOpcode() == InstOpcode::CALL;
}

inline bool RegAlloc::isTerminator(const AsmInst &inst) const {
  switch (inst.getOpcode()) {
  case InstOpcode::RET:
  case InstOpcode::J:
  case InstOpcode::BEQZ:
  case InstOpcode::BNEZ:
  case InstOpcode::BEQ:
  case InstOpcode::BNE:
  case InstOpcode::BLT:
  case InstOpcode::BGE:
  case InstOpcode::BLTU:
  case InstOpcode::BGEU:
    return true;
  default:
    return false;
  }
}

inline const std::vector<int> &
RegAlloc::candidateRegisters(bool crossesCall) const {
  static const std::vector<int> kCrossCallCandidates(kCalleeSavedRegs.begin(),
                                                     kCalleeSavedRegs.end());
  static const std::vector<int> kGeneralCandidates = []() {
    std::vector<int> regs;
    regs.reserve(kCallerSavedRegs.size() + kCalleeSavedRegs.size());
    regs.insert(regs.end(), kCallerSavedRegs.begin(), kCallerSavedRegs.end());
    regs.insert(regs.end(), kCalleeSavedRegs.begin(), kCalleeSavedRegs.end());
    return regs;
  }();

  return crossesCall ? kCrossCallCandidates : kGeneralCandidates;
}

inline bool RegAlloc::overlapsFixedRegister(const FixedRegisterSpan &span,
                                            size_t start, size_t end) const {
  if (!span.valid) {
    return false;
  }
  return !(end < span.start || span.end < start);
}

inline size_t
RegAlloc::blockLabelIndex(const std::vector<std::shared_ptr<AsmOperand>> &uses,
                          InstOpcode opcode) const {
  (void)uses;
  switch (opcode) {
  case InstOpcode::BNEZ:
  case InstOpcode::BEQZ:
    return 1;
  case InstOpcode::BEQ:
  case InstOpcode::BNE:
  case InstOpcode::BLT:
  case InstOpcode::BGE:
  case InstOpcode::BLTU:
  case InstOpcode::BGEU:
    return 2;
  default:
    return 0;
  }
}

inline void
RegAlloc::extendInterval(std::unordered_map<size_t, LiveInterval> &intervals,
                         size_t vregId, size_t start, size_t end) const {
  auto &interval = intervals[vregId];
  if (interval.vregId == 0) {
    interval.vregId = vregId;
    interval.start = start;
    interval.end = end;
    return;
  }

  interval.start = std::min(interval.start, start);
  interval.end = std::max(interval.end, end);
}

inline std::unique_ptr<AsmInst> RegAlloc::cloneInst(
    const AsmInst &inst, const std::shared_ptr<AsmOperand> &dst,
    const std::vector<std::shared_ptr<AsmOperand>> &uses) const {
  if (dst) {
    return std::make_unique<AsmInst>(inst.getOpcode(), dst, uses);
  }
  return std::make_unique<AsmInst>(inst.getOpcode(), uses);
}

} // namespace rc::backend
