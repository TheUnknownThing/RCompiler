#pragma once

#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "opt/base/baseVisitor.hpp"
#include "opt/cfg/cfg.hpp"

#include "utils/logger.hpp"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rc::opt {
class FunctionInline {
public:
  virtual ~FunctionInline() = default;

  void run(ir::Module &module);
  void checkInline(ir::Function &function);
  std::shared_ptr<ir::Value>
  processInline(ir::CallInst &callInst,
                std::shared_ptr<ir::BasicBlock> returnBB);

private:
  bool judgeInline(ir::Function &function);

  static bool canReachFunction(ir::Function *start, ir::Function *target,
                               std::unordered_set<ir::Function *> &visited);

  void mergeMultipleReturns(ir::Function &function);

  static void appendClonedInst(ir::BasicBlock &bb,
                               const std::shared_ptr<ir::Instruction> &inst);

  static void fixOperands(const std::shared_ptr<ir::Instruction> &inst,
                          const ir::ValueRemapMap &valueMap);

  static void replacePhiBlock(ir::Function &function,
                              std::shared_ptr<ir::BasicBlock> oldBB,
                              std::shared_ptr<ir::BasicBlock> newBB);
  static void replaceAllUsesWith(ir::Function &function, ir::Value &from,
                                 ir::Value *to);
};

inline void FunctionInline::run(ir::Module &module) {
  for (auto &function : module.functions()) {
    mergeMultipleReturns(*function);
  }

  for (auto &function : module.functions()) {
    checkInline(*function);
  }

  CFGVisitor cfg;
  cfg.run(module);
}

inline void FunctionInline::mergeMultipleReturns(ir::Function &function) {
  std::vector<std::shared_ptr<ir::ReturnInst>> retInsts;
  for (const auto &block : function.blocks()) {
    for (const auto &inst : block->instructions()) {
      if (auto retInst = std::dynamic_pointer_cast<ir::ReturnInst>(inst)) {
        retInsts.push_back(retInst);
      }
    }
  }

  if (retInsts.size() <= 1) {
    return;
  }

  auto newRetBlock = function.createBlock("merged_return");
  ir::TypePtr retType = ir::VoidType::get();
  if (!function.returnType()->isVoid()) {
    retType = function.returnType();
  }

  for (const auto &oldRet : retInsts) {
    auto parentBlock = oldRet->parent();
    parentBlock->eraseInstruction(oldRet); // remove old return
    auto branchToNewRet =
        std::make_shared<ir::BranchInst>(parentBlock, newRetBlock);
    if (!parentBlock->instructions().empty()) {
      parentBlock->instructions().back()->setNext(branchToNewRet.get());
      branchToNewRet->setPrev(parentBlock->instructions().back().get());
    } else {
      branchToNewRet->setPrev(nullptr);
    }
    branchToNewRet->setNext(nullptr);

    parentBlock->instructions().push_back(branchToNewRet);
  }

  if (retType->isVoid()) {
    auto newRetInst = std::make_shared<ir::ReturnInst>(newRetBlock.get());
    newRetBlock->instructions().push_back(newRetInst);
    newRetInst->setPrev(nullptr);
    newRetInst->setNext(nullptr);
  } else {
    // Create a phi node to select the correct return value
    std::vector<ir::PhiInst::Incoming> incomings;
    for (const auto &oldRet : retInsts) {
      incomings.emplace_back(oldRet->value(),
                             oldRet->parent()->shared_from_this());
    }
    auto phiNode =
        std::make_shared<ir::PhiInst>(newRetBlock.get(), retType, incomings);
    newRetBlock->instructions().push_back(phiNode);

    auto newRetInst =
        std::make_shared<ir::ReturnInst>(newRetBlock.get(), phiNode);
    newRetBlock->instructions().push_back(newRetInst);
  }
}

inline void FunctionInline::checkInline(ir::Function &function) {
  bool changed = true;
  while (changed) {
    changed = false;

    auto blocks = function.blocks();
    for (const auto &block : blocks) {
      if (!block) {
        continue;
      }

      auto &insts = block->instructions();
      for (std::size_t i = 0; i < insts.size(); ++i) {
        const auto inst = insts[i];
        auto *callInst = dynamic_cast<ir::CallInst *>(inst.get());
        if (!callInst) {
          continue;
        }
        if (callInst->parent() != block.get()) {
          continue;
        }

        auto callee = callInst->calleeFunction();
        if (!callee) {
          throw std::runtime_error("Callee function not found when visiting " +
                                   callInst->name() + " in function " +
                                   function.name());
        }

        if (!judgeInline(*callee)) {
          LOG_DEBUG("Not inlining function " + callee->name() +
                    " called from " + function.name());
          continue;
        }

        LOG_DEBUG("Inlining function " + callee->name() + " called from " +
                  function.name());

        auto returnBB = function.splitBlock(block, callInst);
        if (!returnBB) {
          throw std::runtime_error(
              "Failed to split block when inlining function " + callee->name() +
              " called from " + function.name());
        }

        replacePhiBlock(function, block, returnBB);

        auto retVal = processInline(*callInst, returnBB);
        if (retVal) {
          replaceAllUsesWith(function, *callInst, retVal.get());
        }

        block->eraseInstruction(inst);

        changed = true;
        break;
      }

      if (changed) {
        break;
      }
    }
  }
}

inline std::shared_ptr<ir::Value>
FunctionInline::processInline(ir::CallInst &callInst,
                              std::shared_ptr<ir::BasicBlock> returnBB) {
  // return the cloned PHI inst that holds the return value
  auto calleeFunc = callInst.calleeFunction();
  if (!calleeFunc) {
    return nullptr;
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

  auto branchToCallee = std::make_shared<ir::BranchInst>(
      curBlock, blockMap[calleeFunc->blocks().front().get()]);
  if (!curBlock->instructions().empty()) {
    curBlock->instructions().back()->setNext(branchToCallee.get());
    branchToCallee->setPrev(curBlock->instructions().back().get());
  } else {
    branchToCallee->setPrev(nullptr);
  }
  branchToCallee->setNext(nullptr);
  curBlock->instructions().push_back(branchToCallee);

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

  std::shared_ptr<ir::Value> retVal = nullptr;

  for (const auto &block : calleeFunc->blocks()) {
    auto newBlock = blockMap[block.get()];
    for (const auto &inst : newBlock->instructions()) {
      if (auto retInst = std::dynamic_pointer_cast<ir::ReturnInst>(inst)) {
        if (!retInst->isVoid()) {
          // we should only have 1 return per function after merging
          retVal = retInst->value();
        } else {
          retVal = nullptr;
        }

        newBlock->eraseInstruction(retInst);
        auto branchToReturnBB =
            std::make_shared<ir::BranchInst>(newBlock.get(), returnBB);
        if (!newBlock->instructions().empty()) {
          newBlock->instructions().back()->setNext(branchToReturnBB.get());
          branchToReturnBB->setPrev(newBlock->instructions().back().get());
        } else {
          branchToReturnBB->setPrev(nullptr);
        }
        branchToReturnBB->setNext(nullptr);
        newBlock->instructions().push_back(branchToReturnBB);
      }
    }
  }

  return retVal;
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

inline void
FunctionInline::replacePhiBlock(ir::Function &function,
                                std::shared_ptr<ir::BasicBlock> oldBB,
                                std::shared_ptr<ir::BasicBlock> newBB) {
  for (const auto &block : function.blocks()) {
    for (const auto &inst : block->instructions()) {
      if (auto phi = std::dynamic_pointer_cast<ir::PhiInst>(inst)) {
        phi->replaceIncomingBlock(oldBB, newBB);
      }
    }
  }
}

inline void FunctionInline::replaceAllUsesWith(ir::Function &function,
                                               ir::Value &from, ir::Value *to) {
  for (const auto &bb : function.blocks()) {
    if (!bb) {
      continue;
    }
    for (const auto &inst : bb->instructions()) {
      if (!inst) {
        continue;
      }
      inst->replaceOperand(&from, to);
    }
  }
}

inline bool FunctionInline::judgeInline(ir::Function &function) {
  // < 20 instruction & no recursion
  size_t instructionCount = 0;

  if (function.isExternal() || function.blocks().empty()) {
    return false;
  }
  std::unordered_set<ir::Function *> directCallees;

  for (auto &block : function.blocks()) {
    instructionCount += block->instructions().size();
    for (auto &inst : block->instructions()) {
      if (auto callInst = dynamic_cast<ir::CallInst *>(inst.get())) {
        auto callee = callInst->calleeFunction();
        if (callee.get() == &function) {
          return false;
        }

        if (callee && !callee->isExternal() && !callee->blocks().empty()) {
          directCallees.insert(callee.get());
        }
      }
    }
  }
  if (instructionCount > 20 || instructionCount == 0) {
    return false;
  }

  for (auto *callee : directCallees) {
    std::unordered_set<ir::Function *> visited;
    if (canReachFunction(callee, &function, visited)) {
      return false;
    }
  }

  return true;
}

inline bool
FunctionInline::canReachFunction(ir::Function *start, ir::Function *target,
                                 std::unordered_set<ir::Function *> &visited) {
  if (!start || !target) {
    return false;
  }
  if (start == target) {
    return true;
  }
  if (!visited.insert(start).second) {
    return false;
  }

  for (const auto &bb : start->blocks()) {
    for (const auto &inst : bb->instructions()) {
      auto *callInst = dynamic_cast<ir::CallInst *>(inst.get());
      if (!callInst) {
        continue;
      }
      auto callee = callInst->calleeFunction();
      if (!callee || callee->isExternal() || callee->blocks().empty()) {
        continue;
      }
      if (canReachFunction(callee.get(), target, visited)) {
        return true;
      }
    }
  }
  return false;
}

} // namespace rc::opt