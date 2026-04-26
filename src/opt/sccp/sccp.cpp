#include "opt/sccp/sccp.hpp"

namespace rc::opt {

void SCCPVisitor::run(ir::Module &module) {
  for (const auto &function : module.functions()) {
    instruction_worklist_.clear();
    edges_.clear();
    lattice_values_.clear();
    constant_values_.clear();
    executable_blocks_.clear();
    instructions_to_remove_.clear();

    visit(*function);
  }
}

void SCCPVisitor::visit(ir::Value &value) {
  if (auto function = dynamic_cast<ir::Function *>(&value)) {
    visit(*function);
  } else if (auto basic_block = dynamic_cast<ir::BasicBlock *>(&value)) {
    visit(*basic_block);
  } else if (auto branch = dynamic_cast<ir::BranchInst *>(&value)) {
    visit(*branch);
  } else if (auto ret = dynamic_cast<ir::ReturnInst *>(&value)) {
    visit(*ret);
  } else if (auto bin_op = dynamic_cast<ir::BinaryOpInst *>(&value)) {
    visit(*bin_op);
  } else if (auto load = dynamic_cast<ir::LoadInst *>(&value)) {
    visit(*load);
  } else if (auto store = dynamic_cast<ir::StoreInst *>(&value)) {
    visit(*store);
  } else if (auto gep = dynamic_cast<ir::GetElementPtrInst *>(&value)) {
    visit(*gep);
  } else if (auto icmp = dynamic_cast<ir::ICmpInst *>(&value)) {
    visit(*icmp);
  } else if (auto sext = dynamic_cast<ir::SExtInst *>(&value)) {
    visit(*sext);
  } else if (auto zext = dynamic_cast<ir::ZExtInst *>(&value)) {
    visit(*zext);
  } else if (auto trunc = dynamic_cast<ir::TruncInst *>(&value)) {
    visit(*trunc);
  } else if (auto call = dynamic_cast<ir::CallInst *>(&value)) {
    visit(*call);
  } else if (auto phi = dynamic_cast<ir::PhiInst *>(&value)) {
    visit(*phi);
  } else if (auto select = dynamic_cast<ir::SelectInst *>(&value)) {
    visit(*select);
  }
}

void SCCPVisitor::visit(ir::Function &function) {
  LOG_DEBUG("Starting SCCP on function: " + function.name());
  //   executableBlocks_.insert(function.blocks().front().get());

  if (function.blocks().empty()) {
    return;
  }

  auto init_block =
      std::make_shared<ir::BasicBlock>("__sccp_init_block__", &function);
  edges_.push_back(
      std::make_pair(init_block.get(), function.blocks().front().get()));

  for (auto arg : function.args()) {
    lattice_values_[arg.get()] = LatticeValueKind::OVERDEF;
  }

  while (!edges_.empty() || !instruction_worklist_.empty()) {
    // pop controlFlowWorklist first
    while (!edges_.empty()) {
      auto edge = edges_.back();
      edges_.pop_back();
      auto *to_bb = edge.second;
      if (executable_blocks_.count(to_bb) == 0) {
        executable_blocks_.insert(to_bb);
        for (const auto &inst : to_bb->instructions()) {
          if (!inst || dynamic_cast<ir::UnreachableInst *>(inst.get())) {
            break;
          }
          instruction_worklist_.push_back(inst.get());
        }

        if (utils::detail::successors(*to_bb).size() == 1) {
          auto *succ = utils::detail::successors(*to_bb).front();
          edges_.push_back(std::make_pair(to_bb, succ));
        }
      }

      // process PHI nodes
      for (const auto &inst : to_bb->instructions()) {
        if (!inst || dynamic_cast<ir::UnreachableInst *>(inst.get())) {
          break;
        }
        if (auto *phi = dynamic_cast<ir::PhiInst *>(inst.get())) {
          instruction_worklist_.push_back(phi);
        } else {
          break; // PHI nodes are always at the beginning
        }
      }
    }

    while (!instruction_worklist_.empty()) {
      auto *inst = instruction_worklist_.back();
      instruction_worklist_.pop_back();
      visit(*inst);
    }
  }

  // cleanup
  for (auto bb : function.blocks()) {
    for (const auto &inst : bb->instructions()) {
      if (get_lattice_value(inst.get()) == LatticeValueKind::CONSTANT) {
        auto const_val = constant_values_[inst.get()];
        utils::replace_all_uses_with(*inst, const_val.get());
        instructions_to_remove_.insert(inst.get());
      }
    }
  }

  remove_dead_instructions(function);
}

void SCCPVisitor::visit(ir::BasicBlock &basic_block) {
  LOG_DEBUG("SCCP visiting basic block: " + basic_block.name());
  for (const auto &inst : basic_block.instructions()) {
    if (!inst || dynamic_cast<ir::UnreachableInst *>(inst.get())) {
      break;
    }
    instruction_worklist_.push_back(inst.get());
  }
}

void SCCPVisitor::visit(ir::BinaryOpInst &binary_op_inst) {
  LOG_DEBUG("SCCP visiting binary op: " + binary_op_inst.name());
  auto kind = evaluate_kind(binary_op_inst.lhs().get(), binary_op_inst.rhs().get());
  auto prev = get_lattice_value(&binary_op_inst);
  if (kind != prev) {
    if (kind == LatticeValueKind::CONSTANT) {
      auto lhs_const = constant_values_[binary_op_inst.lhs().get()];
      auto rhs_const = constant_values_[binary_op_inst.rhs().get()];

      std::shared_ptr<ir::Constant> result_const = nullptr;
      if (auto lhs_int = std::dynamic_pointer_cast<ir::ConstantInt>(lhs_const)) {
        auto lhs_value = lhs_int->value();
        if (auto rhs_int =
                std::dynamic_pointer_cast<ir::ConstantInt>(rhs_const)) {
          auto rhs_value = rhs_int->value();
          switch (binary_op_inst.op()) {
          case ir::BinaryOpKind::ADD:
            result_const = context_->get_int_constant(
                static_cast<std::int32_t>(lhs_value + rhs_value), false);
            break;
          case ir::BinaryOpKind::SUB:
            result_const = context_->get_int_constant(
                static_cast<std::int32_t>(lhs_value - rhs_value), false);
            break;
          case ir::BinaryOpKind::MUL:
            result_const = context_->get_int_constant(
                static_cast<std::int32_t>(lhs_value * rhs_value), false);
            break;
          case ir::BinaryOpKind::SDIV:
          case ir::BinaryOpKind::UDIV:
            if (rhs_value != 0) {
              result_const = context_->get_int_constant(
                  static_cast<std::int32_t>(lhs_value / rhs_value), false);
            } else {
              kind = LatticeValueKind::OVERDEF;
            }
            break;
          default:
            kind = LatticeValueKind::OVERDEF;
            break;
          }
        } else {
          kind = LatticeValueKind::OVERDEF;
        }
      } else {
        kind = LatticeValueKind::OVERDEF;
      }

      if (kind == LatticeValueKind::CONSTANT) {
        constant_values_[&binary_op_inst] = result_const;
      }
    }

    lattice_values_[&binary_op_inst] = kind;

    for (auto *user : binary_op_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::visit(ir::BranchInst &branch_inst) {
  LOG_DEBUG("SCCP visiting branch instruction");
  if (branch_inst.is_conditional()) {
    auto cond_kind = get_lattice_value(branch_inst.cond().get());
    if (cond_kind == LatticeValueKind::CONSTANT) {
      auto cond_const = constant_values_[branch_inst.cond().get()];
      auto const_bool = std::dynamic_pointer_cast<ir::ConstantInt>(cond_const);
      if (const_bool && const_bool->value() != 0) {
        auto *target_bb = branch_inst.dest();
        edges_.push_back(std::make_pair(branch_inst.parent(), target_bb));
      } else {
        auto *target_bb = branch_inst.alt_dest();
        edges_.push_back(std::make_pair(branch_inst.parent(), target_bb));
      }
    } else if (cond_kind == LatticeValueKind::OVERDEF) {
      auto *target_bb = branch_inst.dest();
      edges_.push_back(std::make_pair(branch_inst.parent(), target_bb));

      auto *alt_bb = branch_inst.alt_dest();
      edges_.push_back(std::make_pair(branch_inst.parent(), alt_bb));
    } else {
      // undef cond, do nothing
      return;
    }
  } else {
    auto *target_bb = branch_inst.dest();
    edges_.push_back(std::make_pair(branch_inst.parent(), target_bb));
  }
}

void SCCPVisitor::visit(ir::UnreachableInst &) {
  LOG_DEBUG("SCCP visiting unreachable instruction");
  // no-op
}

void SCCPVisitor::visit(ir::ReturnInst &) {
  LOG_DEBUG("SCCP visiting return instruction");
  // no-op
}

void SCCPVisitor::visit(ir::AllocaInst &alloca_inst) {
  LOG_DEBUG("SCCP visiting alloca instruction");
  // overdef
  auto prev = get_lattice_value(&alloca_inst);
  if (prev != LatticeValueKind::OVERDEF) {
    lattice_values_[&alloca_inst] = LatticeValueKind::OVERDEF;

    for (auto *user : alloca_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::visit(ir::LoadInst &load_inst) {
  LOG_DEBUG("SCCP visiting load instruction");
  auto prev = get_lattice_value(&load_inst);
  if (auto ptr = dynamic_cast<ir::ConstantPtr *>(load_inst.pointer().get())) {
    lattice_values_[&load_inst] = LatticeValueKind::CONSTANT;
    constant_values_[&load_inst] = ptr->pointee();
    if (prev != LatticeValueKind::CONSTANT) {
      for (auto *user : load_inst.get_uses()) {
        if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
          instruction_worklist_.push_back(inst);
        }
      }
    }
  } else {
    // overdef
    if (prev != LatticeValueKind::OVERDEF) {
      lattice_values_[&load_inst] = LatticeValueKind::OVERDEF;

      for (auto *user : load_inst.get_uses()) {
        if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
          instruction_worklist_.push_back(inst);
        }
      }
    }
  }
}

void SCCPVisitor::visit(ir::StoreInst &) {
  LOG_DEBUG("SCCP visiting store instruction");
  // no-op
}

void SCCPVisitor::visit(ir::GetElementPtrInst &get_element_ptr_inst) {
  LOG_DEBUG("SCCP visiting getelementptr instruction");
  auto prev = get_lattice_value(&get_element_ptr_inst);
  if (auto arr = dynamic_cast<ir::ConstantArray *>(
          get_element_ptr_inst.base_pointer().get())) {
    LOG_DEBUG("GEP base is constant array");
    for (const auto &idx : get_element_ptr_inst.indices()) {
      if (!dynamic_cast<ir::ConstantInt *>(idx.get())) {
        // overdef
        if (prev != LatticeValueKind::OVERDEF) {
          lattice_values_[&get_element_ptr_inst] = LatticeValueKind::OVERDEF;

          for (auto *user : get_element_ptr_inst.get_uses()) {
            if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
              instruction_worklist_.push_back(inst);
            }
          }
        }
        return;
      }
    }
    // could be constant folded
    auto current_unfold = arr;
    for (size_t i = 1; i < get_element_ptr_inst.indices().size(); ++i) {
      // all the middle indices produce constArray
      auto idx_const = std::dynamic_pointer_cast<ir::ConstantInt>(
          get_element_ptr_inst.indices()[i]);
      auto idx_value = idx_const->value();
      if (auto inner_arr = dynamic_cast<ir::ConstantArray *>(
              current_unfold->elements()[idx_value].get())) {
        current_unfold = inner_arr;
      } else if (auto const_elem = dynamic_cast<ir::Constant *>(
                     current_unfold->elements()[idx_value].get())) {
        lattice_values_[&get_element_ptr_inst] = LatticeValueKind::CONSTANT;
        constant_values_[&get_element_ptr_inst] = context_->get_ptr_to_const_element(
            std::dynamic_pointer_cast<ir::Constant>(
                const_elem->shared_from_this()));

        if (prev != LatticeValueKind::CONSTANT) {
          for (auto *user : get_element_ptr_inst.get_uses()) {
            if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
              instruction_worklist_.push_back(inst);
            }
          }
        }
        return;
      } else {
        // overdef
        if (prev != LatticeValueKind::OVERDEF) {
          lattice_values_[&get_element_ptr_inst] = LatticeValueKind::OVERDEF;

          if (prev != LatticeValueKind::OVERDEF) {
            for (auto *user : get_element_ptr_inst.get_uses()) {
              if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
                instruction_worklist_.push_back(inst);
              }
            }
          }
        }
        return;
      }
    }
  } else if (auto ptr = dynamic_cast<ir::ConstantPtr *>(
                 get_element_ptr_inst.base_pointer().get())) {
    LOG_DEBUG("GEP base is constant pointer");
    // could be constant folded
    auto pointee_const = ptr->pointee();
    for (size_t i = 0; i < get_element_ptr_inst.indices().size(); ++i) {
      auto idx_const = std::dynamic_pointer_cast<ir::ConstantInt>(
          get_element_ptr_inst.indices()[i]);
      auto idx_value = idx_const->value();
      if (auto arr = dynamic_cast<ir::ConstantArray *>(pointee_const.get())) {
        pointee_const = arr->elements()[idx_value];
      } else {
        // overdef
        if (prev != LatticeValueKind::OVERDEF) {
          lattice_values_[&get_element_ptr_inst] = LatticeValueKind::OVERDEF;

          for (auto *user : get_element_ptr_inst.get_uses()) {
            if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
              instruction_worklist_.push_back(inst);
            }
          }
        }
        return;
      }
    }
    lattice_values_[&get_element_ptr_inst] = LatticeValueKind::CONSTANT;
    constant_values_[&get_element_ptr_inst] =
        context_->get_ptr_to_const_element(std::dynamic_pointer_cast<ir::Constant>(
            pointee_const->shared_from_this()));

    if (prev != LatticeValueKind::CONSTANT) {
      for (auto *user : get_element_ptr_inst.get_uses()) {
        if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
          instruction_worklist_.push_back(inst);
        }
      }
    }
    return;
  } else {
    // overdef
    if (prev != LatticeValueKind::OVERDEF) {
      lattice_values_[&get_element_ptr_inst] = LatticeValueKind::OVERDEF;

      for (auto *user : get_element_ptr_inst.get_uses()) {
        if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
          instruction_worklist_.push_back(inst);
        }
      }
    }
  }
}

void SCCPVisitor::visit(ir::ICmpInst &icmp_inst) {
  LOG_DEBUG("SCCP visiting icmp instruction");
  auto kind = evaluate_kind(icmp_inst.lhs().get(), icmp_inst.rhs().get());
  auto prev = get_lattice_value(&icmp_inst);
  if (kind != prev) {
    if (kind == LatticeValueKind::CONSTANT) {
      auto lhs_const = constant_values_[icmp_inst.lhs().get()];
      auto rhs_const = constant_values_[icmp_inst.rhs().get()];

      std::shared_ptr<ir::Constant> result_const = nullptr;
      if (auto lhs_int = std::dynamic_pointer_cast<ir::ConstantInt>(lhs_const)) {
        auto lhs_value = lhs_int->value();
        if (auto rhs_int =
                std::dynamic_pointer_cast<ir::ConstantInt>(rhs_const)) {
          auto rhs_value = rhs_int->value();
          bool cmp_result = false;
          switch (icmp_inst.pred()) {
          case ir::ICmpPred::EQ:
            cmp_result = (lhs_value == rhs_value);
            break;
          case ir::ICmpPred::NE:
            cmp_result = (lhs_value != rhs_value);
            break;
          case ir::ICmpPred::SLT:
            cmp_result = (static_cast<std::int32_t>(lhs_value) <
                         static_cast<std::int32_t>(rhs_value));
            break;
          case ir::ICmpPred::SGT:
            cmp_result = (static_cast<std::int32_t>(lhs_value) >
                         static_cast<std::int32_t>(rhs_value));
            break;
          case ir::ICmpPred::SLE:
            cmp_result = (static_cast<std::int32_t>(lhs_value) <=
                         static_cast<std::int32_t>(rhs_value));
            break;
          case ir::ICmpPred::SGE:
            cmp_result = (static_cast<std::int32_t>(lhs_value) >=
                         static_cast<std::int32_t>(rhs_value));
            break;
          case ir::ICmpPred::ULT:
            cmp_result = (lhs_value < rhs_value);
            break;
          case ir::ICmpPred::UGT:
            cmp_result = (lhs_value > rhs_value);
            break;
          case ir::ICmpPred::ULE:
            cmp_result = (lhs_value <= rhs_value);
            break;
          case ir::ICmpPred::UGE:
            cmp_result = (lhs_value >= rhs_value);
            break;
          default:
            kind = LatticeValueKind::OVERDEF;
            break;
          }
          result_const = context_->get_int_constant(cmp_result ? 1 : 0, false);
        } else {
          kind = LatticeValueKind::OVERDEF;
        }
      } else {
        kind = LatticeValueKind::OVERDEF;
      }

      if (kind == LatticeValueKind::CONSTANT) {
        constant_values_[&icmp_inst] = result_const;
      }
    }
    lattice_values_[&icmp_inst] = kind;

    for (auto *user : icmp_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::visit(ir::SExtInst &sext_inst) {
  LOG_DEBUG("SCCP visiting sext instruction");
  auto kind = get_lattice_value(sext_inst.source().get());
  auto prev = get_lattice_value(&sext_inst);
  if (kind != prev) {
    if (kind == LatticeValueKind::CONSTANT) {
      auto src_const = constant_values_[sext_inst.source().get()];
      if (auto src_int = std::dynamic_pointer_cast<ir::ConstantInt>(src_const)) {
        LOG_DEBUG("SCCP folding sext constant");
        auto src_value = src_int->value();
        std::int32_t signed_value = static_cast<std::int32_t>(src_value);
        std::int32_t extended_value = signed_value;
        constant_values_[&sext_inst] = context_->get_int_constant(
            static_cast<std::uint32_t>(extended_value), false);
      } else {
        kind = LatticeValueKind::OVERDEF;
      }
    }

    lattice_values_[&sext_inst] = kind;

    for (auto *user : sext_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::visit(ir::ZExtInst &zext_inst) {
  LOG_DEBUG("SCCP visiting zext instruction");
  auto kind = get_lattice_value(zext_inst.source().get());
  auto prev = get_lattice_value(&zext_inst);
  if (kind != prev) {
    if (kind == LatticeValueKind::CONSTANT) {
      auto src_const = constant_values_[zext_inst.source().get()];
      if (auto src_int = std::dynamic_pointer_cast<ir::ConstantInt>(src_const)) {
        auto src_value = src_int->value();
        LOG_DEBUG("SCCP folding zext constant");
        constant_values_[&zext_inst] = context_->get_int_constant(src_value, false);
      } else {
        kind = LatticeValueKind::OVERDEF;
      }
    }

    lattice_values_[&zext_inst] = kind;

    for (auto *user : zext_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::visit(ir::TruncInst &trunc_inst) {
  LOG_DEBUG("SCCP visiting trunc instruction");
  auto kind = get_lattice_value(trunc_inst.source().get());
  auto prev = get_lattice_value(&trunc_inst);
  if (kind != prev) {
    if (kind == LatticeValueKind::CONSTANT) {
      auto src_const = constant_values_[trunc_inst.source().get()];
      if (auto src_int = std::dynamic_pointer_cast<ir::ConstantInt>(src_const)) {
        LOG_DEBUG("SCCP folding trunc constant");
        auto src_value = src_int->value();
        std::uint32_t truncated_value =
            src_value & ((1u << trunc_inst.dest_bits()) - 1);
        constant_values_[&trunc_inst] =
            context_->get_int_constant(static_cast<int>(truncated_value), false);
      } else {
        kind = LatticeValueKind::OVERDEF;
      }
    }
    lattice_values_[&trunc_inst] = kind;

    for (auto *user : trunc_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::visit(ir::CallInst &call_inst) {
  LOG_DEBUG("SCCP visiting call instruction");
  // overdef
  auto prev = get_lattice_value(&call_inst);
  if (prev != LatticeValueKind::OVERDEF) {
    lattice_values_[&call_inst] = LatticeValueKind::OVERDEF;

    for (auto *user : call_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::visit(ir::PhiInst &phi_inst) {
  LOG_DEBUG("SCCP visiting phi instruction");

  auto prev = get_lattice_value(&phi_inst);
  LatticeValueKind result_kind = LatticeValueKind::UNDEF;
  std::shared_ptr<ir::Constant> const_value = nullptr;
  ir::Value *merged_value = nullptr;

  auto incoming = phi_inst.incomings();
  for (const auto &[value, bb] : incoming) {
    if (executable_blocks_.count(bb) == 0) {
      continue;
    }
    auto kind = get_lattice_value(value.get());
    auto [merged_kind, merged_const] =
        merge_phi_values(result_kind, merged_value, kind, value.get());

    if (merged_kind == LatticeValueKind::OVERDEF) {
      result_kind = LatticeValueKind::OVERDEF;
      const_value = nullptr;
      break;
    }

    result_kind = merged_kind;
    if (result_kind == LatticeValueKind::CONSTANT) {
      const_value = merged_const;
      if (!merged_value) {
        merged_value = value.get();
      }
    } else {
      const_value = nullptr;
      merged_value = nullptr;
    }
  }

  lattice_values_[&phi_inst] = result_kind;
  if (result_kind == LatticeValueKind::CONSTANT) {
    constant_values_[&phi_inst] = const_value;
  }

  if (lattice_values_[&phi_inst] != prev) {

    for (auto *user : phi_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::visit(ir::SelectInst &select_inst) {
  LOG_DEBUG("SCCP visiting select instruction");

  auto prev = get_lattice_value(&select_inst);
  auto cond = get_lattice_value(select_inst.cond().get());
  if (cond == LatticeValueKind::CONSTANT) {
    auto cond_const = constant_values_[select_inst.cond().get()];
    auto cond_int = std::dynamic_pointer_cast<ir::ConstantInt>(cond_const);
    const bool take_true = cond_int && cond_int->value() != 0;
    auto chosen = take_true ? select_inst.if_true() : select_inst.if_false();
    auto chosen_kind = get_lattice_value(chosen.get());
    lattice_values_[&select_inst] = chosen_kind;
    if (chosen_kind == LatticeValueKind::CONSTANT) {
      constant_values_[&select_inst] = constant_values_[chosen.get()];
    }

  } else if (cond == LatticeValueKind::UNDEF) {
    // result is UNDEF
    lattice_values_[&select_inst] = LatticeValueKind::UNDEF;
  } else {
    // cond == overdef, merge
    auto true_kind = get_lattice_value(select_inst.if_true().get());
    auto false_kind = get_lattice_value(select_inst.if_false().get());
    auto [result_kind, const_value] =
        merge_phi_values(true_kind, select_inst.if_true().get(), false_kind,
                       select_inst.if_false().get());
    lattice_values_[&select_inst] = result_kind;
    if (result_kind == LatticeValueKind::CONSTANT) {
      constant_values_[&select_inst] = const_value;
    }
  }

  if (lattice_values_[&select_inst] != prev) {

    for (auto *user : select_inst.get_uses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instruction_worklist_.push_back(inst);
      }
    }
  }
}

void SCCPVisitor::remove_dead_instructions(ir::Function &function) {

  auto instruction_safe_to_remove = [&](ir::Instruction *inst) -> bool {
    // An instruction is safe to remove if it has no side effects.
    if (dynamic_cast<ir::StoreInst *>(inst) ||
        dynamic_cast<ir::CallInst *>(inst) ||
        dynamic_cast<ir::BranchInst *>(inst) ||
        dynamic_cast<ir::ReturnInst *>(inst) ||
        dynamic_cast<ir::UnreachableInst *>(inst)) {
      return false;
    }
    return true;
  };

  for (const auto &bb : function.blocks()) {
    auto &instrs = bb->instructions();
    // NOTE: upon removing the instructions, we also need to adjust its next &
    // prev ptr.
    for (auto it = instrs.begin(); it != instrs.end();) {
      if ((instructions_to_remove_.count(it->get()) || it->get()->get_uses().empty()) &&
          instruction_safe_to_remove(
              std::static_pointer_cast<ir::Instruction>(*it).get())) {
        auto inst = std::static_pointer_cast<ir::Instruction>(*it);
        inst->drop_all_references();

        auto *prev = inst->prev();
        auto *next = inst->next();
        if (prev) {
          prev->set_next(next);
        }
        if (next) {
          next->set_prev(prev);
        }
        it = instrs.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void SCCPVisitor::remove_dead_blocks(ir::Function &function) {

  for (auto &bb : function.blocks()) {
    // remove predecessors' entries to this block
    for (auto *pred : bb->predecessors()) {
      pred->remove_predecessor(bb.get());
    }
  }

  for (auto it = function.blocks().begin(); it != function.blocks().end();) {
    if (executable_blocks_.count(it->get()) == 0) {
      it = function.blocks().erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace rc::opt
