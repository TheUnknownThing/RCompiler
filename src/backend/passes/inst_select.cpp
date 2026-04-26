#include "backend/passes/inst_select.hpp"

namespace rc::backend {

void InstructionSelection::generate(const ir::Module &module) {
  for (const auto &function : module.functions()) {
    if (function->is_external()) {
      continue;
    }
    visit(*function);
  }
}

void InstructionSelection::visit(const ir::Function &function) {
  current_function_ = &function;
  LOG_DEBUG("[ISel] Begin function: " + function.name());

  auto asm_function = std::make_unique<AsmFunction>();
  asm_function->name = get_unique_function_name(&function);
  functions_.push_back(std::move(asm_function));

  stack_offset_ = 0;
  if (!function.blocks().empty()) {
    entry_ir_block_ = function.blocks().front().get();
    for (size_t i = 0; i < function.args().size(); ++i) {
      value_operand_map_[function.args()[i].get()] = create_virtual_register();
    }
    for (const auto &basic_block : function.blocks()) {
      for (const auto &inst : basic_block->instructions()) {
        auto alloca_inst = dynamic_cast<const ir::AllocaInst *>(inst.get());
        if (!alloca_inst || value_operand_map_.count(alloca_inst) != 0) {
          continue;
        }
        value_operand_map_[alloca_inst] =
            create_stack_slot(alloca_inst->allocated_type());
      }
    }
    for (const auto &basic_block : function.blocks()) {
      visit(*basic_block);
    }
  }
  LOG_DEBUG("[ISel] End function: " + function.name());
  functions_.back()->stack_size = align_to(stack_offset_, 16);
  current_function_ = nullptr;
  entry_ir_block_ = nullptr;
}

void InstructionSelection::visit(const ir::BasicBlock &basic_block) {
  current_ir_block_ = &basic_block;

  auto &asm_function = functions_.back();
  std::string label = get_unique_label(&basic_block);
  LOG_DEBUG("[ISel] Enter block: " + label + " (ir=" + basic_block.name() +
            ")");

  auto asm_block = asm_function->create_block(label);
  current_block_ = asm_block;
  if (&basic_block == entry_ir_block_ && current_function_) {
    std::vector<std::shared_ptr<StackSlot>> register_arg_slots;
    const size_t register_arg_count =
        std::min<size_t>(8, current_function_->args().size());
    register_arg_slots.reserve(register_arg_count);
    for (size_t i = 0; i < register_arg_count; ++i) {
      auto slot = std::make_shared<StackSlot>(align_to(stack_offset_, 4), 4);
      stack_offset_ = slot->offset + slot->size;
      register_arg_slots.push_back(slot);
      current_block_->create_inst(InstOpcode::SW, nullptr,
                                  {create_physical_register(10 + i), slot});
    }

    for (size_t i = 0; i < current_function_->args().size(); ++i) {
      auto dst = std::dynamic_pointer_cast<Register>(
          value_operand_map_[current_function_->args()[i].get()]);
      if (!dst) {
        throw std::runtime_error("function argument not mapped to register");
      }

      if (i < 8) {
        current_block_->create_inst(InstOpcode::LW, dst,
                                    {register_arg_slots[i]});
        continue;
      }

      auto slot = create_incoming_arg_slot(i);
      size_t arg_size =
          compute_type_byte_size(current_function_->args()[i]->type());
      if (arg_size == 1) {
        current_block_->create_inst(InstOpcode::LB, dst, {slot});
      } else if (arg_size == 2) {
        current_block_->create_inst(InstOpcode::LH, dst, {slot});
      } else if (arg_size == 4) {
        current_block_->create_inst(InstOpcode::LW, dst, {slot});
      } else {
        throw std::runtime_error("unsupported incoming argument size: " +
                                 std::to_string(arg_size));
      }
    }
  }

  for (const auto &inst : basic_block.instructions()) {
    if (!inst) {
      throw std::runtime_error(
          "InstructionSelection: null instruction in block " + label);
    }

    LOG_DEBUG("[ISel] Select inst name=" +
              (inst->name().empty() ? std::string("<unnamed>") : inst->name()));
    inst->accept(*this);
    if (inst->is_terminator()) {
      break;
    }
  }

  LOG_DEBUG("[ISel] Exit block: " + label);
  current_ir_block_ = nullptr;
}

void InstructionSelection::visit(const ir::BinaryOpInst &bin_op) {
  auto dst = get_or_create_result_register(&bin_op);
  auto lhs = resolve_operand_or_immediate(
      bin_op.lhs(), "LHS operand for binary operation", &bin_op);
  auto rhs = resolve_operand_or_immediate(
      bin_op.rhs(), "RHS operand for binary operation", &bin_op);

  auto imm_fits_shamt = [](int32_t v) -> bool { return v >= 0 && v < 32; };

  switch (bin_op.op()) {
  case rc::ir::BinaryOpKind::ADD: {
    auto *lhs_imm = as_immediate(lhs);
    auto *rhs_imm = as_immediate(rhs);
    if (!lhs_imm && rhs_imm && rhs_imm->is_valid_12()) {
      current_block_->create_inst(InstOpcode::ADDI, dst, {lhs, rhs});
      break;
    }
    if (!rhs_imm && lhs_imm && lhs_imm->is_valid_12()) {
      current_block_->create_inst(InstOpcode::ADDI, dst, {rhs, lhs});
      break;
    }
    if (lhs_imm && rhs_imm) {
      current_block_->create_inst(InstOpcode::LI, dst,
                                  {create_immediate(static_cast<int32_t>(
                                      lhs_imm->value + rhs_imm->value))});
      break;
    }
    current_block_->create_inst(InstOpcode::ADD, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::SUB: {
    if (auto *imm = as_immediate(rhs); imm) {
      int32_t neg64 = -static_cast<int32_t>(imm->value);
      if (neg64 >= -2048 && neg64 <= 2047) {
        auto neg_imm = create_immediate(static_cast<int32_t>(neg64));
        current_block_->create_inst(InstOpcode::ADDI, dst,
                                    {get_reg(lhs), neg_imm});
        break;
      }
    }
    current_block_->create_inst(InstOpcode::SUB, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::MUL: {
    current_block_->create_inst(InstOpcode::MUL, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::SDIV: {
    current_block_->create_inst(InstOpcode::DIV, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::UDIV: {
    current_block_->create_inst(InstOpcode::DIVU, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::SREM: {
    current_block_->create_inst(InstOpcode::REM, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::UREM: {
    current_block_->create_inst(InstOpcode::REMU, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::SHL: {
    if (auto *imm = as_immediate(rhs); imm && imm_fits_shamt(imm->value)) {
      current_block_->create_inst(InstOpcode::SLLI, dst, {get_reg(lhs), rhs});
      break;
    }
    current_block_->create_inst(InstOpcode::SLL, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::ASHR: {
    if (auto *imm = as_immediate(rhs); imm && imm_fits_shamt(imm->value)) {
      current_block_->create_inst(InstOpcode::SRAI, dst, {get_reg(lhs), rhs});
      break;
    }
    current_block_->create_inst(InstOpcode::SRA, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::LSHR: {
    if (auto *imm = as_immediate(rhs); imm && imm_fits_shamt(imm->value)) {
      current_block_->create_inst(InstOpcode::SRLI, dst, {get_reg(lhs), rhs});
      break;
    }
    current_block_->create_inst(InstOpcode::SRL, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::AND: {
    if (auto *imm = as_immediate(rhs);
        imm && imm->is_valid_12() && !as_immediate(lhs)) {
      current_block_->create_inst(InstOpcode::ANDI, dst, {lhs, rhs});
      break;
    }
    if (auto *imm = as_immediate(lhs);
        imm && imm->is_valid_12() && !as_immediate(rhs)) {
      current_block_->create_inst(InstOpcode::ANDI, dst, {rhs, lhs});
      break;
    }
    current_block_->create_inst(InstOpcode::AND, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::OR: {
    if (auto *imm = as_immediate(rhs);
        imm && imm->is_valid_12() && !as_immediate(lhs)) {
      current_block_->create_inst(InstOpcode::ORI, dst, {lhs, rhs});
      break;
    }
    if (auto *imm = as_immediate(lhs);
        imm && imm->is_valid_12() && !as_immediate(rhs)) {
      current_block_->create_inst(InstOpcode::ORI, dst, {rhs, lhs});
      break;
    }
    current_block_->create_inst(InstOpcode::OR, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::XOR: {
    if (auto *imm = as_immediate(rhs);
        imm && imm->is_valid_12() && !as_immediate(lhs)) {
      current_block_->create_inst(InstOpcode::XORI, dst, {lhs, rhs});
      break;
    }
    if (auto *imm = as_immediate(lhs);
        imm && imm->is_valid_12() && !as_immediate(rhs)) {
      current_block_->create_inst(InstOpcode::XORI, dst, {rhs, lhs});
      break;
    }
    current_block_->create_inst(InstOpcode::XOR, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  }
  default:
    throw std::runtime_error("unhandled binary operation");
  }
}

void InstructionSelection::visit(const ir::AllocaInst &alloca) {
  if (value_operand_map_.count(&alloca) != 0) {
    return;
  }
  auto slot = create_stack_slot(alloca.allocated_type());
  value_operand_map_[&alloca] = slot;
}

void InstructionSelection::visit(const ir::LoadInst &load) {
  if (is_aggregate_type(load.type())) {
    throw std::runtime_error("loading aggregate should not exist");
  }
  auto dst = get_or_create_result_register(&load);
  auto ptr = resolve_operand_or_immediate(
      load.pointer(), "load pointer not materialized", &load);
  auto slot = std::dynamic_pointer_cast<StackSlot>(ptr);

  size_t load_size = compute_type_byte_size(load.type());
  if (load_size == 0) {
    throw std::runtime_error("cannot load type with zero byte size");
  }

  if (slot) {
    if (load_size == 1) {
      current_block_->create_inst(InstOpcode::LB, dst, {slot});
    } else if (load_size == 2) {
      current_block_->create_inst(InstOpcode::LH, dst, {slot});
    } else if (load_size == 4) {
      current_block_->create_inst(InstOpcode::LW, dst, {slot});
    } else {
      throw std::runtime_error("unsupported load size: " +
                               std::to_string(load_size));
    }
    return;
  }

  if (ptr->type == OperandType::REG) {
    auto base = get_reg(ptr);
    auto zero = create_immediate(0);
    if (load_size == 1) {
      current_block_->create_inst(InstOpcode::LB, dst, {base, zero});
    } else if (load_size == 2) {
      current_block_->create_inst(InstOpcode::LH, dst, {base, zero});
    } else if (load_size == 4) {
      current_block_->create_inst(InstOpcode::LW, dst, {base, zero});
    } else {
      throw std::runtime_error("unsupported load size: " +
                               std::to_string(load_size));
    }
    return;
  }

  throw std::runtime_error("LoadInst pointer must be stack slot or register");
}

void InstructionSelection::visit(const ir::StoreInst &store) {
  if (is_aggregate_type(store.type())) {
    throw std::runtime_error("storing aggregate should not exist");
  }

  auto src = resolve_operand_or_immediate(store.value(), "store value", &store);
  auto ptr = resolve_operand_or_immediate(
      store.pointer(), "store pointer not materialized", &store);
  auto slot = std::dynamic_pointer_cast<StackSlot>(ptr);

  size_t store_size = compute_type_byte_size(store.value()->type());
  if (store_size == 0) {
    throw std::runtime_error("cannot store type with zero byte size");
  }

  if (slot) {
    if (store_size == 1) {
      current_block_->create_inst(InstOpcode::SB, nullptr,
                                  {get_reg(src), slot});
    } else if (store_size == 2) {
      current_block_->create_inst(InstOpcode::SH, nullptr,
                                  {get_reg(src), slot});
    } else if (store_size == 4) {
      current_block_->create_inst(InstOpcode::SW, nullptr,
                                  {get_reg(src), slot});
    } else {
      throw std::runtime_error("unsupported store size: " +
                               std::to_string(store_size));
    }
    return;
  }

  if (ptr->type == OperandType::REG) {
    auto base = get_reg(ptr);
    auto zero = create_immediate(0);
    if (store_size == 1) {
      current_block_->create_inst(InstOpcode::SB, nullptr,
                                  {get_reg(src), base, zero});
    } else if (store_size == 2) {
      current_block_->create_inst(InstOpcode::SH, nullptr,
                                  {get_reg(src), base, zero});
    } else if (store_size == 4) {
      current_block_->create_inst(InstOpcode::SW, nullptr,
                                  {get_reg(src), base, zero});
    } else {
      throw std::runtime_error("unsupported store size: " +
                               std::to_string(store_size));
    }
    return;
  }

  throw std::runtime_error("StoreInst pointer must be stack slot or register");
}

void InstructionSelection::visit(const ir::GetElementPtrInst &gep) {
  auto base = resolve_operand_or_immediate(
      gep.base_pointer(), "GEP base pointer not materialized", &gep);
  auto current_type = gep.base_pointer()->type();

  int64_t static_offset = 0;
  std::shared_ptr<AsmOperand> dynamic_offset = nullptr;

  auto append_dynamic_offset = [&](const std::shared_ptr<AsmOperand> &part) {
    if (!dynamic_offset) {
      dynamic_offset = part;
      return;
    }
    auto merged = create_virtual_register();
    current_block_->create_inst(InstOpcode::ADD, merged,
                                {get_reg(dynamic_offset), get_reg(part)});
    dynamic_offset = merged;
  };

  for (const auto &index : gep.indices()) {
    if (current_type->kind() == ir::TypeKind::Pointer) {
      auto ptr_ty =
          std::dynamic_pointer_cast<const ir::PointerType>(current_type);
      if (!ptr_ty) {
        throw std::runtime_error("invalid pointer type in GEP");
      }

      size_t stride = compute_type_byte_size(ptr_ty->pointee());
      if (auto const_idx = std::dynamic_pointer_cast<ir::ConstantInt>(index)) {
        static_offset += static_cast<int64_t>(const_idx->value()) *
                         static_cast<int64_t>(stride);
      } else {
        append_dynamic_offset(emit_scaled_index(index, stride, &gep));
      }

      current_type = ptr_ty->pointee();
      continue;
    }

    if (current_type->kind() == ir::TypeKind::Array) {
      auto arr_ty =
          std::dynamic_pointer_cast<const ir::ArrayType>(current_type);
      if (!arr_ty) {
        throw std::runtime_error("invalid array type in GEP");
      }

      auto elem_layout = compute_type_layout(arr_ty->elem());
      size_t stride = align_to(elem_layout.size, elem_layout.align);
      if (auto const_idx = std::dynamic_pointer_cast<ir::ConstantInt>(index)) {
        static_offset += static_cast<int64_t>(const_idx->value()) *
                         static_cast<int64_t>(stride);
      } else {
        append_dynamic_offset(emit_scaled_index(index, stride, &gep));
      }

      current_type = arr_ty->elem();
      continue;
    }

    if (current_type->kind() == ir::TypeKind::Struct) {
      auto struct_ty =
          std::dynamic_pointer_cast<const ir::StructType>(current_type);
      if (!struct_ty) {
        throw std::runtime_error("invalid struct type in GEP");
      }

      auto const_idx = std::dynamic_pointer_cast<ir::ConstantInt>(index);
      if (!const_idx) {
        throw std::runtime_error(
            "non-constant struct GEP index is not supported");
      }

      int64_t idx_value = const_idx->value();
      if (idx_value < 0 ||
          static_cast<size_t>(idx_value) >= struct_ty->fields().size()) {
        throw std::runtime_error("struct GEP index out of bounds");
      }

      size_t field_offset = 0;
      for (size_t field_index = 0; field_index < static_cast<size_t>(idx_value);
           ++field_index) {
        auto field_layout =
            compute_type_layout(struct_ty->fields()[field_index]);
        field_offset = align_to(field_offset, field_layout.align);
        field_offset += field_layout.size;
      }
      auto target_layout = compute_type_layout(struct_ty->fields()[idx_value]);
      field_offset = align_to(field_offset, target_layout.align);

      static_offset += static_cast<int64_t>(field_offset);
      current_type = struct_ty->fields()[idx_value];
      continue;
    }

    throw std::runtime_error("GEP index into non-aggregate type");
  }

  size_t result_size = compute_type_byte_size(current_type);
  if (result_size == 0) {
    throw std::runtime_error("cannot compute GEP with zero-size type");
  }

  auto slot = std::dynamic_pointer_cast<StackSlot>(base);
  if (slot && !dynamic_offset && static_offset >= 0) {
    auto offset = slot->offset + static_cast<size_t>(static_offset);
    auto existing_it = value_operand_map_.find(&gep);
    if (existing_it != value_operand_map_.end() && existing_it->second) {
      auto existing_reg =
          std::dynamic_pointer_cast<Register>(existing_it->second);
      if (!existing_reg) {
        throw std::runtime_error("GEP result mapped to non-register");
      }
      auto sp = create_physical_register(2);
      emit_add_immediate(existing_reg, sp, static_cast<int64_t>(offset));
      return;
    }

    auto new_slot =
        std::make_shared<StackSlot>(offset, result_size, slot->kind);
    value_operand_map_[&gep] = new_slot;
    return;
  }

  auto base_addr_reg =
      materialize_address(base, &gep, "GEP base cannot be addressed");
  std::shared_ptr<Register> address_reg = base_addr_reg;

  if (static_offset != 0) {
    auto with_static = create_virtual_register();
    emit_add_immediate(with_static, address_reg, static_offset);
    address_reg = with_static;
  }

  if (dynamic_offset) {
    auto with_dynamic = create_virtual_register();
    current_block_->create_inst(InstOpcode::ADD, with_dynamic,
                                {address_reg, get_reg(dynamic_offset)});
    address_reg = with_dynamic;
  }

  auto existing_it = value_operand_map_.find(&gep);
  if (existing_it != value_operand_map_.end() && existing_it->second) {
    auto existing_reg =
        std::dynamic_pointer_cast<Register>(existing_it->second);
    if (!existing_reg) {
      throw std::runtime_error("GEP result mapped to non-register");
    }
    if (existing_reg != address_reg) {
      current_block_->create_inst(InstOpcode::MV, existing_reg, {address_reg});
    }
    return;
  }

  value_operand_map_[&gep] = address_reg;
}

std::shared_ptr<Register> InstructionSelection::materialize_address(
    const std::shared_ptr<AsmOperand> &ptr, const ir::Instruction *inst,
    const char *reason) {
  if (!ptr) {
    throw std::runtime_error(std::string(reason) + ": null pointer operand");
  }

  if (ptr->type == OperandType::REG) {
    return std::static_pointer_cast<Register>(ptr);
  }

  if (auto slot = std::dynamic_pointer_cast<StackSlot>(ptr)) {
    auto addr = create_virtual_register();
    auto sp = create_physical_register(2);
    emit_add_immediate(addr, sp, static_cast<int64_t>(slot->offset));
    return addr;
  }

  throw std::runtime_error(std::string(reason) + ": " +
                           (inst ? describe(inst) : std::string("<no inst>")));
}

void InstructionSelection::emit_add_immediate(
    const std::shared_ptr<Register> &dst,
    const std::shared_ptr<AsmOperand> &lhs, int64_t imm_value) {
  if (imm_value < static_cast<int64_t>(std::numeric_limits<int32_t>::min()) ||
      imm_value > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
    throw std::runtime_error("offset exceeds 32-bit immediate range");
  }

  auto imm = create_immediate(static_cast<int32_t>(imm_value));
  if (imm->is_valid_12()) {
    current_block_->create_inst(InstOpcode::ADDI, dst, {get_reg(lhs), imm});
    return;
  }

  current_block_->create_inst(InstOpcode::ADD, dst,
                              {get_reg(lhs), get_reg(imm)});
}

std::shared_ptr<AsmOperand>
InstructionSelection::emit_scaled_index(const std::shared_ptr<ir::Value> &index,
                                        size_t stride,
                                        const ir::Instruction *inst) {
  auto idx =
      resolve_operand_or_immediate(index, "GEP index not materialized", inst);
  if (stride == 1) {
    return idx;
  }

  if (auto imm = as_immediate(idx)) {
    int64_t scaled =
        static_cast<int64_t>(imm->value) * static_cast<int64_t>(stride);
    if (scaled < static_cast<int64_t>(std::numeric_limits<int32_t>::min()) ||
        scaled > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
      throw std::runtime_error("scaled GEP immediate offset out of range");
    }
    return create_immediate(static_cast<int32_t>(scaled));
  }

  auto index_reg = get_reg(idx);

  if ((stride & (stride - 1)) == 0 && stride < 32) {
    size_t shift = 0;
    while ((1u << shift) != stride) {
      ++shift;
    }
    auto scaled_reg = create_virtual_register();
    current_block_->create_inst(
        InstOpcode::SLLI, scaled_reg,
        {index_reg, create_immediate(static_cast<int32_t>(shift))});
    return scaled_reg;
  }

  if (stride > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    throw std::runtime_error("GEP stride exceeds 32-bit immediate range");
  }

  auto scaled_reg = create_virtual_register();
  auto stride_imm = create_immediate(static_cast<int32_t>(stride));
  current_block_->create_inst(InstOpcode::MUL, scaled_reg,
                              {index_reg, get_reg(stride_imm)});
  return scaled_reg;
}

void InstructionSelection::visit(const ir::BranchInst &branch) {
  if (branch.is_conditional()) {
    auto cond = resolve_operand_or_immediate(
        branch.cond(), "conditional branch condition not materialized",
        &branch);
    current_block_->create_inst(InstOpcode::BNEZ, nullptr,
                                {get_reg(cond),
                                 create_label_operand(branch.dest()),
                                 create_label_operand(branch.alt_dest())});
  } else {
    if (!branch.dest()) {
      throw std::runtime_error("unconditional branch with null destination");
    }
    current_block_->create_inst(InstOpcode::J, nullptr,
                                {create_label_operand(branch.dest())});
  }
}

void InstructionSelection::visit(const ir::ReturnInst &ret) {
  if (!ret.is_void()) {
    auto ret_val = resolve_operand_or_immediate(
        ret.value(), "return value not materialized", &ret);
    auto dst = create_physical_register(10); // a0
    current_block_->create_inst(InstOpcode::MV, dst, {get_reg(ret_val)});
  }
  current_block_->create_inst(InstOpcode::RET, nullptr, {});
}

void InstructionSelection::visit(const ir::UnreachableInst &) {}
void InstructionSelection::visit(const ir::ICmpInst &icmp) {
  auto dst = get_or_create_result_register(&icmp);
  auto lhs = resolve_operand_or_immediate(icmp.lhs(),
                                          "LHS operand for ICmpInst", &icmp);
  auto rhs = resolve_operand_or_immediate(icmp.rhs(),
                                          "RHS operand for ICmpInst", &icmp);
  switch (icmp.pred()) {
  case ir::ICmpPred::EQ:
    // rd = (rs1 ^ rs2) == 0
    current_block_->create_inst(InstOpcode::XOR, dst,
                                {get_reg(lhs), get_reg(rhs)});
    current_block_->create_inst(InstOpcode::SLTIU, dst,
                                {get_reg(dst), create_immediate(1)});
    break;
  case ir::ICmpPred::NE:
    // rd = (rs1 ^ rs2) != 0
    current_block_->create_inst(InstOpcode::XOR, dst,
                                {get_reg(lhs), get_reg(rhs)});
    current_block_->create_inst(InstOpcode::SLTU, dst,
                                {create_physical_register(0), get_reg(dst)});
    break;
  case ir::ICmpPred::UGT:
    current_block_->create_inst(InstOpcode::SLTU, dst,
                                {get_reg(rhs), get_reg(lhs)});
    break;
  case ir::ICmpPred::SGT:
    current_block_->create_inst(InstOpcode::SLT, dst,
                                {get_reg(rhs), get_reg(lhs)});
    break;
  case ir::ICmpPred::ULT:
    current_block_->create_inst(InstOpcode::SLTU, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  case ir::ICmpPred::SLT:
    current_block_->create_inst(InstOpcode::SLT, dst,
                                {get_reg(lhs), get_reg(rhs)});
    break;
  case ir::ICmpPred::UGE:
    current_block_->create_inst(InstOpcode::SLTU, dst,
                                {get_reg(lhs), get_reg(rhs)});
    current_block_->create_inst(InstOpcode::XORI, dst,
                                {get_reg(dst), create_immediate(1)});
    break;
  case ir::ICmpPred::SGE:
    current_block_->create_inst(InstOpcode::SLT, dst,
                                {get_reg(lhs), get_reg(rhs)});
    current_block_->create_inst(InstOpcode::XORI, dst,
                                {get_reg(dst), create_immediate(1)});
    break;
  case ir::ICmpPred::ULE:
    current_block_->create_inst(InstOpcode::SLTU, dst,
                                {get_reg(rhs), get_reg(lhs)});
    current_block_->create_inst(InstOpcode::XORI, dst,
                                {get_reg(dst), create_immediate(1)});
    break;
  case ir::ICmpPred::SLE:
    current_block_->create_inst(InstOpcode::SLT, dst,
                                {get_reg(rhs), get_reg(lhs)});
    current_block_->create_inst(InstOpcode::XORI, dst,
                                {get_reg(dst), create_immediate(1)});
    break;
  default:
    throw std::runtime_error("unknown ICmp predicate");
  }
}

void InstructionSelection::visit(const ir::CallInst &call) {
  std::vector<std::shared_ptr<AsmOperand>> args;
  args.reserve(call.args().size());
  for (const auto &arg : call.args()) {
    args.push_back(prepare_call_argument(arg, call));
  }

  std::vector<std::shared_ptr<Register>> arg_temps;
  arg_temps.reserve(args.size());
  for (const auto &arg : args) {
    auto temp = create_virtual_register();
    current_block_->create_inst(InstOpcode::MV, temp, {get_reg(arg)});
    arg_temps.push_back(temp);
  }

  const size_t stack_arg_count = args.size() > 8 ? args.size() - 8 : 0;
  const size_t stack_arg_bytes = align_to(stack_arg_count * 4, 16);
  auto sp = create_physical_register(2);

  std::shared_ptr<Register> outgoing_sp;
  if (stack_arg_bytes != 0) {
    outgoing_sp = create_virtual_register();
    emit_add_immediate(outgoing_sp, sp, -static_cast<int64_t>(stack_arg_bytes));
    for (size_t i = 8; i < arg_temps.size(); ++i) {
      current_block_->create_inst(
          InstOpcode::SW, nullptr,
          {arg_temps[i], outgoing_sp,
           create_immediate(static_cast<int32_t>((i - 8) * 4))});
    }
  }

  for (size_t i = 0; i < arg_temps.size() && i < 8; ++i) {
    current_block_->create_inst(
        InstOpcode::MV, create_physical_register(10 + i), {arg_temps[i]});
  }

  if (stack_arg_bytes != 0) {
    current_block_->create_inst(InstOpcode::MV, sp, {outgoing_sp});
  }

  current_block_->create_inst(
      InstOpcode::CALL, nullptr,
      {create_function_operand(call.callee_function())});

  if (stack_arg_bytes != 0) {
    emit_add_immediate(sp, sp, static_cast<int64_t>(stack_arg_bytes));
  }

  if (!call.type()->is_void()) {
    current_block_->create_inst(InstOpcode::MV,
                                get_or_create_result_register(&call),
                                {create_physical_register(10)});
  }
}

void InstructionSelection::visit(const ir::PhiInst &) {
  throw std::runtime_error("PhiInst should have been removed by now");
}

void InstructionSelection::visit(const ir::SelectInst &) {
  // We do not emit SelectInst previously, so leave blank
}

void InstructionSelection::visit(const ir::ZExtInst &zext_inst) {
  auto dst = get_or_create_result_register(&zext_inst);
  auto src = resolve_operand_or_immediate(zext_inst.source(),
                                          "operand for ZExtInst", &zext_inst);
  auto src_int = std::dynamic_pointer_cast<const ir::IntegerType>(
      zext_inst.source()->type());
  if (!src_int || src_int->bits() >= 32) {
    current_block_->create_inst(InstOpcode::MV, dst, {get_reg(src)});
    return;
  }
  current_block_->create_inst(
      InstOpcode::ANDI, dst,
      {get_reg(src), create_immediate(static_cast<int32_t>(
                         (uint32_t{1} << src_int->bits()) - 1))});
}

void InstructionSelection::visit(const ir::SExtInst &sext_inst) {
  auto dst = get_or_create_result_register(&sext_inst);
  auto src = resolve_operand_or_immediate(sext_inst.source(),
                                          "operand for SExtInst", &sext_inst);
  current_block_->create_inst(
      InstOpcode::SLLI, dst,
      {get_reg(src), create_immediate(32 - sext_inst.src_bits())});
  current_block_->create_inst(
      InstOpcode::SRAI, dst,
      {get_reg(dst), create_immediate(32 - sext_inst.src_bits())});
}

void InstructionSelection::visit(const ir::TruncInst &trunc_inst) {
  auto dst = get_or_create_result_register(&trunc_inst);
  auto src = resolve_operand_or_immediate(trunc_inst.source(),
                                          "operand for TruncInst", &trunc_inst);
  current_block_->create_inst(
      InstOpcode::ANDI, dst,
      {get_reg(src), create_immediate((1u << trunc_inst.dest_bits()) - 1)});
}

void InstructionSelection::visit(const ir::MoveInst &move_inst) {
  // addi rd, rs, 0
  std::shared_ptr<AsmOperand> dst = nullptr;
  if (value_operand_map_.find(move_inst.destination().get()) !=
      value_operand_map_.end()) {
    dst = value_operand_map_[move_inst.destination().get()];
  } else {
    dst = create_virtual_register();
    value_operand_map_[move_inst.destination().get()] = dst;
  }

  auto src = resolve_operand_or_immediate(move_inst.source(),
                                          "operand for MoveInst", &move_inst);
  current_block_->create_inst(InstOpcode::ADDI, dst,
                              {get_reg(src), create_immediate(0)});
}

std::string InstructionSelection::describe(const ir::Value *value) const {
  if (!value) {
    return "<null value>";
  }

  std::ostringstream oss;
  oss << "name=" << (value->name().empty() ? "<unnamed>" : value->name())
      << ", ptr=" << value
      << ", kind=" << static_cast<int>(value->type()->kind());
  return oss.str();
}

std::shared_ptr<AsmOperand> InstructionSelection::resolve_operand_or_immediate(
    const std::shared_ptr<ir::Value> &value, const char *reason,
    const ir::Instruction *inst) {
  (void)inst;
  if (!value) {
    throw std::runtime_error(std::string(reason) + ": null value");
  }

  auto it = value_operand_map_.find(value.get());
  if (it != value_operand_map_.end() && it->second) {
    return it->second;
  }

  if (auto const_int = std::dynamic_pointer_cast<ir::ConstantInt>(value)) {
    return create_immediate(const_int->value());
  }

  if (std::dynamic_pointer_cast<ir::UndefValue>(value)) {
    LOG_WARN("[ISel] materializing undef as 0");
    return create_immediate(0);
  }

  if (std::dynamic_pointer_cast<ir::Argument>(value)) {
    throw std::runtime_error(std::string(reason) +
                             " is a function argument that has not been mapped "
                             "to a register or stack slot: " +
                             describe(value.get()));
  }

  if (std::dynamic_pointer_cast<ir::Instruction>(value)) {
    return get_or_create_result_register(value.get());
  }

  throw std::runtime_error(std::string(reason) + ": " + describe(value.get()));
}

std::shared_ptr<Register> InstructionSelection::create_virtual_register() {
  auto reg = std::make_shared<Register>();
  reg->id = reg_id_++;
  reg->is_virtual = true;
  return reg;
}

std::shared_ptr<Register>
InstructionSelection::get_or_create_result_register(const ir::Value *value) {
  auto it = value_operand_map_.find(value);
  if (it != value_operand_map_.end() && it->second) {
    auto reg = std::dynamic_pointer_cast<Register>(it->second);
    if (!reg) {
      throw std::runtime_error("instruction result mapped to non-register");
    }
    return reg;
  }

  auto reg = create_virtual_register();
  value_operand_map_[value] = reg;
  return reg;
}

std::shared_ptr<Register>
InstructionSelection::create_physical_register(int id) {
  auto reg = std::make_shared<Register>();
  reg->id = id;
  reg->is_virtual = false;
  return reg;
}

std::shared_ptr<Immediate>
InstructionSelection::create_immediate(int32_t value) {
  return std::make_shared<Immediate>(value);
}

std::shared_ptr<StackSlot>
InstructionSelection::create_stack_slot(const ir::TypePtr &type) {
  auto layout = compute_type_layout(type);
  stack_offset_ = align_to(stack_offset_, layout.align);
  auto slot = std::make_shared<StackSlot>(stack_offset_, layout.size);
  stack_offset_ += layout.size;
  return slot;
}

std::shared_ptr<StackSlot>
InstructionSelection::create_incoming_arg_slot(size_t arg_index) {
  if (arg_index < 8) {
    throw std::runtime_error("register argument requested as stack argument");
  }
  return std::make_shared<StackSlot>((arg_index - 8) * 4, 4,
                                     StackSlotKind::INCOMING_ARG);
}

std::shared_ptr<Symbol>
InstructionSelection::create_label_operand(const ir::BasicBlock *block) {
  return std::make_shared<Symbol>(get_unique_label(block), false);
}

std::shared_ptr<Symbol>
InstructionSelection::create_function_operand(const ir::Function *function) {
  return std::make_shared<Symbol>(get_unique_function_name(function), true);
}

bool InstructionSelection::is_aggregate_type(const ir::TypePtr &ty) const {
  return ty->kind() == ir::TypeKind::Array ||
         ty->kind() == ir::TypeKind::Struct;
}

size_t InstructionSelection::align_to(size_t offset, size_t align) const {
  if (align <= 1) {
    return offset;
  }
  auto rem = offset % align;
  return rem ? (offset + (align - rem)) : offset;
}

InstructionSelection::TypeLayoutInfo
InstructionSelection::compute_type_layout(const ir::TypePtr &ty) const {
  if (auto int_ty = std::dynamic_pointer_cast<const ir::IntegerType>(ty)) {
    size_t size = (int_ty->bits() + 7) / 8;
    if (size == 0) {
      size = 1;
    }
    return {size, size};
  }
  if (auto arr_ty = std::dynamic_pointer_cast<const ir::ArrayType>(ty)) {
    auto elem = compute_type_layout(arr_ty->elem());
    size_t stride = align_to(elem.size, elem.align);
    return {stride * arr_ty->count(), elem.align};
  }
  if (auto struct_ty = std::dynamic_pointer_cast<const ir::StructType>(ty)) {
    size_t offset = 0;
    size_t max_align = 1;
    for (const auto &field : struct_ty->fields()) {
      auto layout = compute_type_layout(field);
      offset = align_to(offset, layout.align);
      offset += layout.size;
      max_align = std::max(max_align, layout.align);
    }
    offset = align_to(offset, max_align);
    return {offset, max_align};
  }
  if (std::dynamic_pointer_cast<const ir::PointerType>(ty)) {
    size_t ptr_bytes = 4;
    return {ptr_bytes, ptr_bytes};
  }
  if (ty->is_void()) {
    return {0, 1};
  }
  throw std::runtime_error("InstructionSelection: unknown type");
}

size_t
InstructionSelection::compute_type_byte_size(const ir::TypePtr &ty) const {
  return compute_type_layout(ty).size;
}

Immediate *
InstructionSelection::as_immediate(const std::shared_ptr<AsmOperand> &op) {
  if (!op || op->type != OperandType::IMM) {
    return nullptr;
  }
  return static_cast<Immediate *>(op.get());
}

std::shared_ptr<AsmOperand>
InstructionSelection::get_reg(const std::shared_ptr<AsmOperand> &op) {
  if (!as_immediate(op)) {
    if (std::dynamic_pointer_cast<StackSlot>(op)) {
      return materialize_address(op, nullptr, "stack slot address");
    }
    return op;
  }
  auto imm_reg = create_virtual_register();
  current_block_->create_inst(InstOpcode::LI, imm_reg, {op});
  return imm_reg;
}

std::shared_ptr<AsmOperand> InstructionSelection::prepare_call_argument(
    const std::shared_ptr<ir::Value> &arg, const ir::CallInst &call) {
  auto operand = resolve_operand_or_immediate(
      arg, "call argument not materialized", &call);
  if (arg && arg->type()->kind() == ir::TypeKind::Pointer &&
      std::dynamic_pointer_cast<StackSlot>(operand)) {
    return materialize_address(operand, &call,
                               "call pointer argument cannot be addressed");
  }
  return operand;
}

} // namespace rc::backend
