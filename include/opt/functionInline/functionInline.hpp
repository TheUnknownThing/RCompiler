#pragma once

#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "opt/base/baseVisitor.hpp"

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











} // namespace rc::opt