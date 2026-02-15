#pragma once

#include "backend/nodes/instructions.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace rc::backend {

class PseudoAsmEmitter {
public:
  void emit(const std::vector<std::unique_ptr<AsmFunction>> &functions,
            std::ostream &os) const;

private:
  std::string opcodeName(InstOpcode opcode) const;
  std::string operandName(const std::shared_ptr<AsmOperand> &operand) const;
  std::string instName(const AsmInst &inst) const;
};

inline void PseudoAsmEmitter::emit(
    const std::vector<std::unique_ptr<AsmFunction>> &functions,
    std::ostream &os) const {
  os << "# ---- pseudo asm ----\n";
  for (const auto &function : functions) {
    os << ".func " << function->name << " stack=" << function->stackSize
       << "\n";

    if (!function->blocks.empty()) {
      for (const auto &block : function->blocks) {
        os << block->name << ":\n";
        for (const auto &inst : block->instructions) {
          os << "  " << instName(*inst) << "\n";
        }
      }
    }

    os << ".endfunc\n";
  }
  os << "# ---- end pseudo asm ----\n";
}

inline std::string PseudoAsmEmitter::opcodeName(InstOpcode opcode) const {
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
  return "<unknown-opcode>";
}

inline std::string PseudoAsmEmitter::operandName(
    const std::shared_ptr<AsmOperand> &operand) const {
  if (!operand) {
    return "<null>";
  }

  switch (operand->type) {
  case OperandType::REG: {
    auto reg = static_cast<Register *>(operand.get());
    if (reg->is_virtual) {
      return "v" + std::to_string(reg->id);
    }
    return "x" + std::to_string(reg->id);
  }
  case OperandType::IMM: {
    auto imm = static_cast<Immediate *>(operand.get());
    return std::to_string(imm->value);
  }
  case OperandType::SYMBOL: {
    auto symbol = static_cast<Symbol *>(operand.get());
    return symbol->name;
  }
  case OperandType::STACK_SLOT: {
    auto slot = static_cast<StackSlot *>(operand.get());
    return "stack[off=" + std::to_string(slot->offset) +
           ",size=" + std::to_string(slot->size) + "]";
  }
  }

  return "<unknown-operand>";
}

inline std::string PseudoAsmEmitter::instName(const AsmInst &inst) const {
  std::string result = opcodeName(inst.getOpcode());

  std::vector<std::string> operands;
  if (inst.getDst()) {
    operands.push_back(operandName(inst.getDst()));
  }
  for (const auto &use : inst.getUses()) {
    operands.push_back(operandName(use));
  }

  if (!operands.empty()) {
    result += " ";
    for (size_t index = 0; index < operands.size(); ++index) {
      if (index != 0) {
        result += ", ";
      }
      result += operands[index];
    }
  }

  return result;
}

} // namespace rc::backend
