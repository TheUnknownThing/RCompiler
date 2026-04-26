#pragma once

#include "backend/nodes/instructions.hpp"

#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rc::backend {

class AsmEmitter {
public:
  void emit(const std::vector<std::unique_ptr<AsmFunction>> &functions,
            std::ostream &os) const;

private:
  mutable size_t longBranchId_{0};

  std::string opcodeName(InstOpcode opcode) const;
  std::string regName(const std::shared_ptr<AsmOperand> &operand) const;
  std::string symbolName(const std::shared_ptr<AsmOperand> &operand) const;
  int32_t immediateValue(const std::shared_ptr<AsmOperand> &operand) const;
  size_t stackOffset(const AsmFunction &function,
                     const std::shared_ptr<AsmOperand> &operand) const;
  bool fitsSigned12(int64_t value) const;
  void emitLoad(const AsmFunction &function, std::ostream &os,
                const std::string &mnemonic,
                const std::shared_ptr<AsmOperand> &dst,
                const std::shared_ptr<AsmOperand> &slotOrBase,
                const std::shared_ptr<AsmOperand> &maybeOffset) const;
  void emitStore(const AsmFunction &function, std::ostream &os,
                 const std::string &mnemonic,
                 const std::shared_ptr<AsmOperand> &src,
                 const std::shared_ptr<AsmOperand> &slotOrBase,
                 const std::shared_ptr<AsmOperand> &maybeOffset) const;
  void emitInst(const AsmFunction &function, const AsmInst &inst,
                std::ostream &os) const;
};

inline void
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
          emitInst(*function, *inst, os);
        }
      }
    }
  }
}

inline std::string AsmEmitter::opcodeName(InstOpcode opcode) const {
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

inline std::string
AsmEmitter::regName(const std::shared_ptr<AsmOperand> &operand) const {
  auto reg = std::dynamic_pointer_cast<Register>(operand);
  if (!reg || reg->is_virtual) {
    throw std::runtime_error("expected allocated physical register");
  }
  return "x" + std::to_string(reg->id);
}

inline std::string
AsmEmitter::symbolName(const std::shared_ptr<AsmOperand> &operand) const {
  auto symbol = std::dynamic_pointer_cast<Symbol>(operand);
  if (!symbol) {
    throw std::runtime_error("expected symbol operand");
  }
  return symbol->name;
}

inline int32_t
AsmEmitter::immediateValue(const std::shared_ptr<AsmOperand> &operand) const {
  auto imm = std::dynamic_pointer_cast<Immediate>(operand);
  if (!imm) {
    throw std::runtime_error("expected immediate operand");
  }
  return imm->value;
}

inline size_t
AsmEmitter::stackOffset(const AsmFunction &function,
                        const std::shared_ptr<AsmOperand> &operand) const {
  auto slot = std::dynamic_pointer_cast<StackSlot>(operand);
  if (!slot) {
    throw std::runtime_error("expected stack slot operand");
  }
  if (slot->kind == StackSlotKind::INCOMING_ARG) {
    return function.stackSize + slot->offset;
  }
  return slot->offset;
}

inline bool AsmEmitter::fitsSigned12(int64_t value) const {
  return value >= -2048 && value <= 2047;
}

inline void AsmEmitter::emitLoad(
    const AsmFunction &function, std::ostream &os, const std::string &mnemonic,
    const std::shared_ptr<AsmOperand> &dst,
    const std::shared_ptr<AsmOperand> &slotOrBase,
    const std::shared_ptr<AsmOperand> &maybeOffset) const {
  if (std::dynamic_pointer_cast<StackSlot>(slotOrBase)) {
    auto offset = static_cast<int64_t>(stackOffset(function, slotOrBase));
    if (fitsSigned12(offset)) {
      os << "\t" << mnemonic << "\t" << regName(dst) << ", " << offset
         << "(x2)\n";
      return;
    }
    os << "\tli\tx5, " << offset << "\n";
    os << "\tadd\tx5, x2, x5\n";
    os << "\t" << mnemonic << "\t" << regName(dst) << ", 0(x5)\n";
    return;
  }

  auto offset = static_cast<int64_t>(immediateValue(maybeOffset));
  if (!fitsSigned12(offset)) {
    os << "\tli\tx5, " << offset << "\n";
    os << "\tadd\tx5, " << regName(slotOrBase) << ", x5\n";
    os << "\t" << mnemonic << "\t" << regName(dst) << ", 0(x5)\n";
    return;
  }

  os << "\t" << mnemonic << "\t" << regName(dst) << ", "
     << offset << "(" << regName(slotOrBase) << ")\n";
}

inline void AsmEmitter::emitStore(
    const AsmFunction &function, std::ostream &os, const std::string &mnemonic,
    const std::shared_ptr<AsmOperand> &src,
    const std::shared_ptr<AsmOperand> &slotOrBase,
    const std::shared_ptr<AsmOperand> &maybeOffset) const {
  if (std::dynamic_pointer_cast<StackSlot>(slotOrBase)) {
    auto offset = static_cast<int64_t>(stackOffset(function, slotOrBase));
    if (fitsSigned12(offset)) {
      os << "\t" << mnemonic << "\t" << regName(src) << ", " << offset
         << "(x2)\n";
      return;
    }
    os << "\tli\tx5, " << offset << "\n";
    os << "\tadd\tx5, x2, x5\n";
    os << "\t" << mnemonic << "\t" << regName(src) << ", 0(x5)\n";
    return;
  }

  auto offset = static_cast<int64_t>(immediateValue(maybeOffset));
  if (!fitsSigned12(offset)) {
    os << "\tli\tx5, " << offset << "\n";
    os << "\tadd\tx5, " << regName(slotOrBase) << ", x5\n";
    os << "\t" << mnemonic << "\t" << regName(src) << ", 0(x5)\n";
    return;
  }

  os << "\t" << mnemonic << "\t" << regName(src) << ", "
     << offset << "(" << regName(slotOrBase) << ")\n";
}

inline void AsmEmitter::emitInst(const AsmFunction &function,
                                 const AsmInst &inst,
                                 std::ostream &os) const {
  const auto opcode = inst.getOpcode();
  const auto &uses = inst.getUses();

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
    os << "\t" << opcodeName(opcode) << "\t" << regName(inst.getDst())
       << ", " << regName(uses.at(0)) << ", " << regName(uses.at(1)) << "\n";
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
    os << "\t" << opcodeName(opcode) << "\t" << regName(inst.getDst())
       << ", " << regName(uses.at(0)) << ", " << immediateValue(uses.at(1))
       << "\n";
    return;

  case InstOpcode::LB:
  case InstOpcode::LH:
  case InstOpcode::LW:
  case InstOpcode::LBU:
  case InstOpcode::LHU:
    emitLoad(function, os, opcodeName(opcode), inst.getDst(), uses.at(0),
             uses.size() > 1 ? uses.at(1) : nullptr);
    return;

  case InstOpcode::SB:
  case InstOpcode::SH:
  case InstOpcode::SW:
    emitStore(function, os, opcodeName(opcode), uses.at(0), uses.at(1),
              uses.size() > 2 ? uses.at(2) : nullptr);
    return;

  case InstOpcode::BEQ:
  case InstOpcode::BNE:
  case InstOpcode::BLT:
  case InstOpcode::BGE:
  case InstOpcode::BLTU:
  case InstOpcode::BGEU:
    os << "\t" << opcodeName(opcode) << "\t" << regName(uses.at(0)) << ", "
       << regName(uses.at(1)) << ", " << symbolName(uses.at(2)) << "\n";
    return;

  case InstOpcode::BEQZ:
  case InstOpcode::BNEZ:
    if (uses.size() >= 3) {
      std::string bridge = ".long_branch" + std::to_string(longBranchId_++);
      os << "\t" << opcodeName(opcode) << "\t" << regName(uses.at(0)) << ", "
         << bridge << "\n";
      os << "\tj\t" << symbolName(uses.at(2)) << "\n";
      os << bridge << ":\n";
      os << "\tj\t" << symbolName(uses.at(1)) << "\n";
      return;
    } else {
      throw std::runtime_error("we shouldn't have this case");
    }
    return;

  case InstOpcode::J:
    os << "\tj\t" << symbolName(uses.at(0)) << "\n";
    return;

  case InstOpcode::LI:
    os << "\tli\t" << regName(inst.getDst()) << ", "
       << immediateValue(uses.at(0)) << "\n";
    return;

  case InstOpcode::MV:
    os << "\tmv\t" << regName(inst.getDst()) << ", " << regName(uses.at(0))
       << "\n";
    return;

  case InstOpcode::CALL:
    os << "\tcall\t" << symbolName(uses.at(0)) << "\n";
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
