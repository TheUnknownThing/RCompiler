#pragma once

#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "opt/base/baseVisitor.hpp"

#include "utils/logger.hpp"

#include <memory>
#include <unordered_map>

namespace rc::opt {
class FunctionInline {
public:
  virtual ~FunctionInline() = default;

  void run(ir::Module &module);
  void visit(ir::Function &function);
  void visit(ir::CallInst &callInst);

private:
  bool judgeInline(ir::Function &function);

  static void appendClonedInst(ir::BasicBlock &bb,
                               const std::shared_ptr<ir::Instruction> &inst);

  static void fixOperands(const std::shared_ptr<ir::Instruction> &inst,
                          const ir::ValueRemapMap &valueMap);
};

inline void FunctionInline::run(ir::Module &module) {
  for (auto &function : module.functions()) {
    visit(*function);
  }
}

inline void FunctionInline::visit(ir::Function &function) {
  auto oldBlocks = function.blocks();
  for (const auto &block : oldBlocks) {
    auto oldInstructions = block->instructions();
    for (const auto &inst : oldInstructions) {
      auto *callInst = dynamic_cast<ir::CallInst *>(inst.get());
      if (!callInst) {
        continue;
      }

      auto callee = callInst->calleeFunction();
      if (!callee) {
        throw std::runtime_error("Callee function not found when visiting " +
                                 callInst->name() + " in function " +
                                 function.name());
      }

      if (judgeInline(*callee)) {
        LOG_DEBUG("Inlining function " + callee->name() + " called from " +
                  function.name());
        visit(*callInst);
      } else {
        LOG_DEBUG("Not inlining function " + callee->name() + " called from " +
                  function.name());
      }
    }
  }
}

inline void FunctionInline::visit(ir::CallInst &callInst) {
  auto calleeFunc = callInst.calleeFunction();
  if (!calleeFunc) {
    return;
  }

  ir::ValueRemapMap valueMap;
  ir::BlockRemapMap blockMap;
  const auto &args = callInst.args();
  const auto &params = calleeFunc->params();

  for (size_t i = 0; i < args.size(); ++i) {
    valueMap[params[i].get()] = args[i];
  }

  auto curBlock = callInst.parent();
  auto curFunc = curBlock->parent();

  for (const auto &block : calleeFunc->blocks()) {
    auto newBlock = curFunc->createBlock(block->name() + "_inlined");
    blockMap[block.get()] = newBlock;
  }

  for (const auto &block : calleeFunc->blocks()) {
    auto newBlock = blockMap[block.get()];
    for (const auto &inst : block->instructions()) {
      auto cloned = inst->cloneInst(newBlock.get(), valueMap, blockMap);
      if (!cloned) {
        continue;
      }
      valueMap[inst.get()] = cloned;
      appendClonedInst(*newBlock, cloned);
    }
  }

  for (const auto &block : calleeFunc->blocks()) {
    auto newBlock = blockMap[block.get()];
    for (const auto &inst : newBlock->instructions()) {
      fixOperands(inst, valueMap);
    }
  }
}

inline void
FunctionInline::appendClonedInst(ir::BasicBlock &bb,
                                 const std::shared_ptr<ir::Instruction> &inst) {
  if (!inst) {
    return;
  }
  auto &insts = bb.instructions();
  inst->setPrev(insts.empty() ? nullptr : insts.back().get());
  if (!insts.empty()) {
    insts.back()->setNext(inst.get());
  }
  inst->setNext(nullptr);
  insts.push_back(inst);
}

inline void
FunctionInline::fixOperands(const std::shared_ptr<ir::Instruction> &inst,
                            const ir::ValueRemapMap &valueMap) {
  if (!inst) {
    return;
  }

  for (auto *op : inst->getOperands()) {
    if (!op) {
      continue;
    }
    auto it = valueMap.find(op);
    if (it != valueMap.end()) {
      inst->replaceOperand(op, it->second.get());
    }
  }

  if (auto phi = std::dynamic_pointer_cast<ir::PhiInst>(inst)) {
    for (const auto &inc : phi->incomings()) {
      auto *oldV = inc.first.get();
      if (!oldV) {
        continue;
      }
      auto it = valueMap.find(oldV);
      if (it != valueMap.end()) {
        phi->replaceOperand(oldV, it->second.get());
      }
    }
  }
}

inline bool FunctionInline::judgeInline(ir::Function &function) {
  // < 20 instruction & no recursion
  size_t instructionCount = 0;

  if (function.isExternal() || function.blocks().empty()) {
    return false;
  }

  for (auto &block : function.blocks()) {
    instructionCount += block->instructions().size();
    for (auto &inst : block->instructions()) {
      if (auto callInst = dynamic_cast<ir::CallInst *>(inst.get())) {
        if (callInst->calleeFunction().get() == &function) {
          return false;
        }
      }
    }
  }
  if (instructionCount > 20 || instructionCount == 0) {
    return false;
  }
  return true;
}

} // namespace rc::opt