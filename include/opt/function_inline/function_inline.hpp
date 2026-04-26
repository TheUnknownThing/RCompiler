#pragma once

#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"

#include "opt/base/base_visitor.hpp"

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
  void check_inline(ir::Function &function);
  std::shared_ptr<ir::Value>
  process_inline(ir::CallInst &call_inst,
                std::shared_ptr<ir::BasicBlock> return_bb);

private:
  bool judge_inline(ir::Function &function);

  static bool can_reach_function(ir::Function *start, ir::Function *target,
                               std::unordered_set<ir::Function *> &visited);

  void merge_multiple_returns(ir::Function &function);

  static void append_cloned_inst(ir::BasicBlock &bb,
                               const std::shared_ptr<ir::Instruction> &inst);

  static void fix_operands(const std::shared_ptr<ir::Instruction> &inst,
                          const ir::ValueRemapMap &value_map);

  static void replace_phi_block(ir::Function &function,
                              std::shared_ptr<ir::BasicBlock> old_bb,
                              std::shared_ptr<ir::BasicBlock> new_bb);
  static void replace_all_uses_with(ir::Function &function, ir::Value &from,
                                 ir::Value *to);
};

} // namespace rc::opt