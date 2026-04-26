#include "opt/function_inline/function_inline.hpp"

namespace rc::opt {

void FunctionInline::run(ir::Module &module) {
  for (auto &function : module.functions()) {
    merge_multiple_returns(*function);
  }

  for (auto &function : module.functions()) {
    check_inline(*function);
  }
}
void FunctionInline::merge_multiple_returns(ir::Function &function) {
  std::vector<std::shared_ptr<ir::ReturnInst>> ret_insts;
  for (const auto &block : function.blocks()) {
    for (const auto &inst : block->instructions()) {
      if (auto ret_inst = std::dynamic_pointer_cast<ir::ReturnInst>(inst)) {
        ret_insts.push_back(ret_inst);
      }
    }
  }

  if (ret_insts.size() <= 1) {
    return;
  }

  auto new_ret_block = function.create_block("merged_return");
  ir::TypePtr ret_type = ir::VoidType::get();
  if (!function.return_type()->is_void()) {
    ret_type = function.return_type();
  }

  for (const auto &old_ret : ret_insts) {
    auto parent_block = old_ret->parent();
    parent_block->erase_instruction(old_ret); // remove old return
    auto branch_to_new_ret =
        std::make_shared<ir::BranchInst>(parent_block, new_ret_block.get());
    if (!parent_block->instructions().empty()) {
      parent_block->instructions().back()->set_next(branch_to_new_ret.get());
      branch_to_new_ret->set_prev(parent_block->instructions().back().get());
    } else {
      branch_to_new_ret->set_prev(nullptr);
    }
    branch_to_new_ret->set_next(nullptr);

    parent_block->instructions().push_back(branch_to_new_ret);
  }

  if (ret_type->is_void()) {
    auto new_ret_inst = std::make_shared<ir::ReturnInst>(new_ret_block.get());
    new_ret_block->instructions().push_back(new_ret_inst);
    new_ret_inst->set_prev(nullptr);
    new_ret_inst->set_next(nullptr);
  } else {
    // Create a phi node to select the correct return value
    std::vector<ir::PhiInst::Incoming> incomings;
    for (const auto &old_ret : ret_insts) {
      incomings.emplace_back(old_ret->value(), old_ret->parent());
    }
    auto phi_node =
        std::make_shared<ir::PhiInst>(new_ret_block.get(), ret_type, incomings);
    new_ret_block->instructions().push_back(phi_node);

    auto new_ret_inst =
        std::make_shared<ir::ReturnInst>(new_ret_block.get(), phi_node);
    new_ret_block->instructions().push_back(new_ret_inst);
  }
}
void FunctionInline::check_inline(ir::Function &function) {
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
        auto *call_inst = dynamic_cast<ir::CallInst *>(inst.get());
        if (!call_inst) {
          continue;
        }
        if (call_inst->parent() != block.get()) {
          continue;
        }

        auto *callee = call_inst->callee_function();
        if (!callee) {
          throw std::runtime_error("Callee function not found when visiting " +
                                   call_inst->name() + " in function " +
                                   function.name());
        }

        if (!judge_inline(*callee)) {
          LOG_DEBUG("Not inlining function " + callee->name() +
                    " called from " + function.name());
          continue;
        }

        LOG_DEBUG("Inlining function " + callee->name() + " called from " +
                  function.name());

        auto return_bb = function.split_block(block, call_inst);
        if (!return_bb) {
          throw std::runtime_error(
              "Failed to split block when inlining function " + callee->name() +
              " called from " + function.name());
        }

        replace_phi_block(function, block, return_bb);

        auto ret_val = process_inline(*call_inst, return_bb);
        if (ret_val) {
          replace_all_uses_with(function, *call_inst, ret_val.get());
        }

        block->erase_instruction(inst);

        changed = true;
        break;
      }

      if (changed) {
        break;
      }
    }
  }
}
std::shared_ptr<ir::Value>
FunctionInline::process_inline(ir::CallInst &call_inst,
                              std::shared_ptr<ir::BasicBlock> return_bb) {
  // return the cloned PHI inst that holds the return value
  auto *callee_func = call_inst.callee_function();
  if (!callee_func) {
    return nullptr;
  }

  ir::ValueRemapMap value_map;
  ir::BlockRemapMap block_map;
  const auto &args = call_inst.args();
  const auto &params = callee_func->params();

  for (size_t i = 0; i < args.size(); ++i) {
    value_map[params[i].get()] = args[i];
  }

  auto cur_block = call_inst.parent();
  auto cur_func = cur_block->parent();

  for (const auto &block : callee_func->blocks()) {
    auto new_block = cur_func->create_block(block->name() + "_inlined");
    block_map[block.get()] = new_block.get();
  }

  auto branch_to_callee = std::make_shared<ir::BranchInst>(
      cur_block, block_map[callee_func->blocks().front().get()]);
  if (!cur_block->instructions().empty()) {
    cur_block->instructions().back()->set_next(branch_to_callee.get());
    branch_to_callee->set_prev(cur_block->instructions().back().get());
  } else {
    branch_to_callee->set_prev(nullptr);
  }
  branch_to_callee->set_next(nullptr);
  cur_block->instructions().push_back(branch_to_callee);

  for (const auto &block : callee_func->blocks()) {
    auto *new_block = block_map[block.get()];
    for (const auto &inst : block->instructions()) {
      auto cloned = inst->clone_inst(new_block, value_map, block_map);
      if (!cloned) {
        continue;
      }
      value_map[inst.get()] = cloned;
      append_cloned_inst(*new_block, cloned);
    }
  }

  for (const auto &block : callee_func->blocks()) {
    auto *new_block = block_map[block.get()];
    for (const auto &inst : new_block->instructions()) {
      fix_operands(inst, value_map);
    }
  }

  std::shared_ptr<ir::Value> ret_val = nullptr;

  for (const auto &block : callee_func->blocks()) {
    auto *new_block = block_map[block.get()];
    for (const auto &inst : new_block->instructions()) {
      if (auto ret_inst = std::dynamic_pointer_cast<ir::ReturnInst>(inst)) {
        if (!ret_inst->is_void()) {
          // we should only have 1 return per function after merging
          ret_val = ret_inst->value();
        } else {
          ret_val = nullptr;
        }

        new_block->erase_instruction(ret_inst);
        auto branch_to_return_bb =
            std::make_shared<ir::BranchInst>(new_block, return_bb.get());
        if (!new_block->instructions().empty()) {
          new_block->instructions().back()->set_next(branch_to_return_bb.get());
          branch_to_return_bb->set_prev(new_block->instructions().back().get());
        } else {
          branch_to_return_bb->set_prev(nullptr);
        }
        branch_to_return_bb->set_next(nullptr);
        new_block->instructions().push_back(branch_to_return_bb);
      }
    }
  }

  return ret_val;
}
void
FunctionInline::append_cloned_inst(ir::BasicBlock &bb,
                                 const std::shared_ptr<ir::Instruction> &inst) {
  if (!inst) {
    return;
  }
  auto &insts = bb.instructions();
  inst->set_prev(insts.empty() ? nullptr : insts.back().get());
  if (!insts.empty()) {
    insts.back()->set_next(inst.get());
  }
  inst->set_next(nullptr);
  insts.push_back(inst);
}
void
FunctionInline::fix_operands(const std::shared_ptr<ir::Instruction> &inst,
                            const ir::ValueRemapMap &value_map) {
  if (!inst) {
    return;
  }

  for (auto *op : inst->get_operands()) {
    if (!op) {
      continue;
    }
    auto it = value_map.find(op);
    if (it != value_map.end()) {
      inst->replace_operand(op, it->second.get());
    }
  }

  if (auto phi = std::dynamic_pointer_cast<ir::PhiInst>(inst)) {
    for (const auto &inc : phi->incomings()) {
      auto *old_v = inc.first.get();
      if (!old_v) {
        continue;
      }
      auto it = value_map.find(old_v);
      if (it != value_map.end()) {
        phi->replace_operand(old_v, it->second.get());
      }
    }
  }
}
void
FunctionInline::replace_phi_block(ir::Function &function,
                                std::shared_ptr<ir::BasicBlock> old_bb,
                                std::shared_ptr<ir::BasicBlock> new_bb) {
  for (const auto &block : function.blocks()) {
    for (const auto &inst : block->instructions()) {
      if (auto phi = std::dynamic_pointer_cast<ir::PhiInst>(inst)) {
        phi->replace_incoming_block(old_bb.get(), new_bb.get());
      }
    }
  }
}
void FunctionInline::replace_all_uses_with(ir::Function &function,
                                               ir::Value &from, ir::Value *to) {
  for (const auto &bb : function.blocks()) {
    if (!bb) {
      continue;
    }
    for (const auto &inst : bb->instructions()) {
      if (!inst) {
        continue;
      }
      inst->replace_operand(&from, to);
    }
  }
}
bool FunctionInline::judge_inline(ir::Function &function) {
  // < 20 instruction & no recursion
  size_t instruction_count = 0;

  if (function.is_external() || function.blocks().empty()) {
    return false;
  }
  std::unordered_set<ir::Function *> direct_callees;

  for (auto &block : function.blocks()) {
    instruction_count += block->instructions().size();
    for (auto &inst : block->instructions()) {
      if (auto call_inst = dynamic_cast<ir::CallInst *>(inst.get())) {
        auto *callee = call_inst->callee_function();
        if (callee == &function) {
          return false;
        }

        if (callee && !callee->is_external() && !callee->blocks().empty()) {
          direct_callees.insert(callee);
        }
      }
    }
  }
  if (instruction_count > 20 || instruction_count == 0) {
    return false;
  }

  for (auto *callee : direct_callees) {
    std::unordered_set<ir::Function *> visited;
    if (can_reach_function(callee, &function, visited)) {
      return false;
    }
  }

  return true;
}
bool
FunctionInline::can_reach_function(ir::Function *start, ir::Function *target,
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
      auto *call_inst = dynamic_cast<ir::CallInst *>(inst.get());
      if (!call_inst) {
        continue;
      }
      auto callee = call_inst->callee_function();
      if (!callee || callee->is_external() || callee->blocks().empty()) {
        continue;
      }
      if (can_reach_function(callee, target, visited)) {
        return true;
      }
    }
  }
  return false;
}

} // namespace rc::opt
