#include "backend/passes/asm_emitter.hpp"

namespace rc::backend {

void
AsmEmitter::emit(const std::vector<std::unique_ptr<AsmFunction>> &functions,
                 std::ostream &os) const {
  os << "\t.text\n";
  for (const auto &function : functions) {
    if (!function) {
      continue;
    }

    os << "\t.globl\t" << function->name << "\n";
    os << function->name << ":\n";
    for (const auto &block : function->blocks) {
      if (!block) {
        continue;
      }
      os << block->name << ":\n";
      for (const auto &inst : block->instructions) {
        if (inst) {
          emit_inst(*function, *inst, os);
        }
      }
    }
  }
}

std::string AsmEmitter::opcode_name(InstOpcode opcode) const {
  switch (opcode) {
  case InstOpcode::ADD:
    return "add";
  case InstOpcode::SUB:
    return "sub";
  case InstOpcode::XOR:
    return "xor";
  case InstOpcode::OR:
    return "or";
  case InstOpcode::AND:
    return "and";
  case InstOpcode::SLL:
    return "sll";
  case InstOpcode::SRL:
    return "srl";
  case InstOpcode::SRA:
    return "sra";
  case InstOpcode::SLT:
    return "slt";
  case InstOpcode::SLTU:
    return "sltu";
  case InstOpcode::MUL:
    return "mul";
  case InstOpcode::DIV:
    return "div";
  case InstOpcode::DIVU:
    return "divu";
  case InstOpcode::REM:
    return "rem";
  case InstOpcode::REMU:
    return "remu";
  case InstOpcode::ADDI:
    return "addi";
  case InstOpcode::XORI:
    return "xori";
  case InstOpcode::ORI:
    return "ori";
  case InstOpcode::ANDI:
    return "andi";
  case InstOpcode::SLLI:
    return "slli";
  case InstOpcode::SRLI:
    return "srli";
  case InstOpcode::SRAI:
    return "srai";
  case InstOpcode::SLTI:
    return "slti";
  case InstOpcode::SLTIU:
    return "sltiu";
  case InstOpcode::LB:
    return "lb";
  case InstOpcode::LH:
    return "lh";
  case InstOpcode::LW:
    return "lw";
  case InstOpcode::LBU:
    return "lbu";
  case InstOpcode::LHU:
    return "lhu";
  case InstOpcode::JALR:
    return "jalr";
  case InstOpcode::SB:
    return "sb";
  case InstOpcode::SH:
    return "sh";
  case InstOpcode::SW:
    return "sw";
  case InstOpcode::BEQ:
    return "beq";
  case InstOpcode::BNE:
    return "bne";
  case InstOpcode::BLT:
    return "blt";
  case InstOpcode::BGE:
    return "bge";
  case InstOpcode::BLTU:
    return "bltu";
  case InstOpcode::BGEU:
    return "bgeu";
  case InstOpcode::LUI:
    return "lui";
  case InstOpcode::AUIPC:
    return "auipc";
  case InstOpcode::JAL:
    return "jal";
  case InstOpcode::MV:
    return "mv";
  case InstOpcode::LI:
    return "li";
  case InstOpcode::CALL:
    return "call";
  case InstOpcode::RET:
    return "ret";
  case InstOpcode::BEQZ:
    return "beqz";
  case InstOpcode::BNEZ:
    return "bnez";
  case InstOpcode::J:
    return "j";
  }
  throw std::runtime_error("unknown opcode");
}

std::string
AsmEmitter::reg_name(const std::shared_ptr<AsmOperand> &operand) const {
  auto reg = std::dynamic_pointer_cast<Register>(operand);
  if (!reg || reg->is_virtual) {
    throw std::runtime_error("expected allocated physical register");
  }
  return "x" + std::to_string(reg->id);
}

std::string
AsmEmitter::symbol_name(const std::shared_ptr<AsmOperand> &operand) const {
  auto symbol = std::dynamic_pointer_cast<Symbol>(operand);
  if (!symbol) {
    throw std::runtime_error("expected symbol operand");
  }
  return symbol->name;
}

int32_t
AsmEmitter::immediate_value(const std::shared_ptr<AsmOperand> &operand) const {
  auto imm = std::dynamic_pointer_cast<Immediate>(operand);
  if (!imm) {
    throw std::runtime_error("expected immediate operand");
  }
  return imm->value;
}

size_t
AsmEmitter::stack_offset(const AsmFunction &function,
                        const std::shared_ptr<AsmOperand> &operand) const {
  auto slot = std::dynamic_pointer_cast<StackSlot>(operand);
  if (!slot) {
    throw std::runtime_error("expected stack slot operand");
  }
  if (slot->kind == StackSlotKind::INCOMING_ARG) {
    return function.stack_size + slot->offset;
  }
  return slot->offset;
}

bool AsmEmitter::fits_signed12(int64_t value) const {
  return value >= -2048 && value <= 2047;
}

void AsmEmitter::emit_load(
    const AsmFunction &function, std::ostream &os, const std::string &mnemonic,
    const std::shared_ptr<AsmOperand> &dst,
    const std::shared_ptr<AsmOperand> &slot_or_base,
    const std::shared_ptr<AsmOperand> &maybe_offset) const {
  if (std::dynamic_pointer_cast<StackSlot>(slot_or_base)) {
    auto offset = static_cast<int64_t>(stack_offset(function, slot_or_base));
    if (fits_signed12(offset)) {
      os << "\t" << mnemonic << "\t" << reg_name(dst) << ", " << offset
         << "(x2)\n";
      return;
    }
    os << "\tli\tx5, " << offset << "\n";
    os << "\tadd\tx5, x2, x5\n";
    os << "\t" << mnemonic << "\t" << reg_name(dst) << ", 0(x5)\n";
    return;
  }

  auto offset = static_cast<int64_t>(immediate_value(maybe_offset));
  if (!fits_signed12(offset)) {
    os << "\tli\tx5, " << offset << "\n";
    os << "\tadd\tx5, " << reg_name(slot_or_base) << ", x5\n";
    os << "\t" << mnemonic << "\t" << reg_name(dst) << ", 0(x5)\n";
    return;
  }

  os << "\t" << mnemonic << "\t" << reg_name(dst) << ", "
     << offset << "(" << reg_name(slot_or_base) << ")\n";
}

void AsmEmitter::emit_store(
    const AsmFunction &function, std::ostream &os, const std::string &mnemonic,
    const std::shared_ptr<AsmOperand> &src,
    const std::shared_ptr<AsmOperand> &slot_or_base,
    const std::shared_ptr<AsmOperand> &maybe_offset) const {
  if (std::dynamic_pointer_cast<StackSlot>(slot_or_base)) {
    auto offset = static_cast<int64_t>(stack_offset(function, slot_or_base));
    if (fits_signed12(offset)) {
      os << "\t" << mnemonic << "\t" << reg_name(src) << ", " << offset
         << "(x2)\n";
      return;
    }
    os << "\tli\tx5, " << offset << "\n";
    os << "\tadd\tx5, x2, x5\n";
    os << "\t" << mnemonic << "\t" << reg_name(src) << ", 0(x5)\n";
    return;
  }

  auto offset = static_cast<int64_t>(immediate_value(maybe_offset));
  if (!fits_signed12(offset)) {
    os << "\tli\tx5, " << offset << "\n";
    os << "\tadd\tx5, " << reg_name(slot_or_base) << ", x5\n";
    os << "\t" << mnemonic << "\t" << reg_name(src) << ", 0(x5)\n";
    return;
  }

  os << "\t" << mnemonic << "\t" << reg_name(src) << ", "
     << offset << "(" << reg_name(slot_or_base) << ")\n";
}

void AsmEmitter::emit_inst(const AsmFunction &function,
                                 const AsmInst &inst,
                                 std::ostream &os) const {
  const auto opcode = inst.get_opcode();
  const auto &uses = inst.get_uses();

  switch (opcode) {
  case InstOpcode::ADD:
  case InstOpcode::SUB:
  case InstOpcode::XOR:
  case InstOpcode::OR:
  case InstOpcode::AND:
  case InstOpcode::SLL:
  case InstOpcode::SRL:
  case InstOpcode::SRA:
  case InstOpcode::SLT:
  case InstOpcode::SLTU:
  case InstOpcode::MUL:
  case InstOpcode::DIV:
  case InstOpcode::DIVU:
  case InstOpcode::REM:
  case InstOpcode::REMU:
    os << "\t" << opcode_name(opcode) << "\t" << reg_name(inst.get_dst())
       << ", " << reg_name(uses.at(0)) << ", " << reg_name(uses.at(1)) << "\n";
    return;

  case InstOpcode::ADDI:
  case InstOpcode::XORI:
  case InstOpcode::ORI:
  case InstOpcode::ANDI:
  case InstOpcode::SLLI:
  case InstOpcode::SRLI:
  case InstOpcode::SRAI:
  case InstOpcode::SLTI:
  case InstOpcode::SLTIU:
    os << "\t" << opcode_name(opcode) << "\t" << reg_name(inst.get_dst())
       << ", " << reg_name(uses.at(0)) << ", " << immediate_value(uses.at(1))
       << "\n";
    return;

  case InstOpcode::LB:
  case InstOpcode::LH:
  case InstOpcode::LW:
  case InstOpcode::LBU:
  case InstOpcode::LHU:
    emit_load(function, os, opcode_name(opcode), inst.get_dst(), uses.at(0),
             uses.size() > 1 ? uses.at(1) : nullptr);
    return;

  case InstOpcode::SB:
  case InstOpcode::SH:
  case InstOpcode::SW:
    emit_store(function, os, opcode_name(opcode), uses.at(0), uses.at(1),
              uses.size() > 2 ? uses.at(2) : nullptr);
    return;

  case InstOpcode::BEQ:
  case InstOpcode::BNE:
  case InstOpcode::BLT:
  case InstOpcode::BGE:
  case InstOpcode::BLTU:
  case InstOpcode::BGEU:
    os << "\t" << opcode_name(opcode) << "\t" << reg_name(uses.at(0)) << ", "
       << reg_name(uses.at(1)) << ", " << symbol_name(uses.at(2)) << "\n";
    return;

  case InstOpcode::BEQZ:
  case InstOpcode::BNEZ:
    if (uses.size() >= 3) {
      std::string bridge = ".long_branch" + std::to_string(long_branch_id_++);
      os << "\t" << opcode_name(opcode) << "\t" << reg_name(uses.at(0)) << ", "
         << bridge << "\n";
      os << "\tj\t" << symbol_name(uses.at(2)) << "\n";
      os << bridge << ":\n";
      os << "\tj\t" << symbol_name(uses.at(1)) << "\n";
      return;
    } else {
      throw std::runtime_error("we shouldn't have this case");
    }
    return;

  case InstOpcode::J:
    os << "\tj\t" << symbol_name(uses.at(0)) << "\n";
    return;

  case InstOpcode::LI:
    os << "\tli\t" << reg_name(inst.get_dst()) << ", "
       << immediate_value(uses.at(0)) << "\n";
    return;

  case InstOpcode::MV:
    os << "\tmv\t" << reg_name(inst.get_dst()) << ", " << reg_name(uses.at(0))
       << "\n";
    return;

  case InstOpcode::CALL:
    os << "\tcall\t" << symbol_name(uses.at(0)) << "\n";
    return;

  case InstOpcode::RET:
    os << "\tret\n";
    return;

  case InstOpcode::LUI:
  case InstOpcode::AUIPC:
  case InstOpcode::JAL:
  case InstOpcode::JALR:
    throw std::runtime_error("emitter opcode is not currently generated");
  }
}

} // namespace rc::backend
