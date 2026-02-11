#pragma once

#include "operands.hpp"
#include <memory>
#include <vector>

namespace rc::backend {

enum class InstOpcode {
  // R-Type
  ADD,
  SUB,
  XOR,
  OR,
  AND,
  SLL,
  SRL,
  SRA,
  SLT,
  SLTU,
  // I-Type
  ADDI,
  XORI,
  ORI,
  ANDI,
  SLLI,
  SRLI,
  SRAI,
  SLTI,
  SLTIU,
  LW,
  JALR,
  // S-Type
  SW,
  // B-Type
  BEQ,
  BNE,
  BLT,
  BGE,
  BLTU,
  BGEU,
  // U-Type / J-Type
  LUI,
  AUIPC,
  JAL,
  // PseudoOps
  MV,
  LI,
  CALL,
  RET,
  J
};

class AsmInst {
public:
  AsmInst(InstOpcode opc, std::shared_ptr<AsmOperand> dst,
          std::vector<AsmOperand *> uses)
      : opcode(opc), dst(std::move(dst)), uses(std::move(uses)) {}

  AsmInst(InstOpcode opc, std::vector<AsmOperand *> uses)
      : opcode(opc), dst(nullptr), uses(std::move(uses)) {}

  bool isPseudo() const {
    switch (opcode) {
    case InstOpcode::MV:
    case InstOpcode::LI:
    case InstOpcode::CALL:
    case InstOpcode::RET:
    case InstOpcode::J:
      return true;
    default:
      return false;
    }
  }

private:
  InstOpcode opcode;
  std::shared_ptr<AsmOperand> dst;
  std::vector<AsmOperand *> uses;
};

} // namespace rc::backend
