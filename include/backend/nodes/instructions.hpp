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
  // RV32M (R-Type)
  MUL,
  DIV,
  DIVU,
  REM,
  REMU,
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
  BEQZ,
  BNEZ,
  J
};

class AsmInst {
public:
  AsmInst(InstOpcode opc, std::shared_ptr<AsmOperand> dst,
          std::vector<std::shared_ptr<AsmOperand>> uses)
      : opcode(opc), dst(std::move(dst)), uses(std::move(uses)) {}

  AsmInst(InstOpcode opc, std::vector<std::shared_ptr<AsmOperand>> uses)
      : opcode(opc), dst(nullptr), uses(std::move(uses)) {}

  bool isPseudo() const {
    switch (opcode) {
    case InstOpcode::MV:
    case InstOpcode::LI:
    case InstOpcode::CALL:
    case InstOpcode::RET:
    case InstOpcode::BEQZ:
    case InstOpcode::BNEZ:
    case InstOpcode::J:
      return true;
    default:
      return false;
    }
  }

private:
  InstOpcode opcode;
  std::shared_ptr<AsmOperand> dst;
  std::vector<std::shared_ptr<AsmOperand>> uses;
};

class AsmBlock {
public:
  explicit AsmBlock(std::string name) : name(std::move(name)) {}

  std::string name;
  std::vector<std::unique_ptr<AsmInst>> instructions;

  AsmInst *createInst(InstOpcode opc, std::shared_ptr<AsmOperand> dst,
                      std::vector<std::shared_ptr<AsmOperand>> uses) {
    auto inst = std::make_unique<AsmInst>(opc, std::move(dst), std::move(uses));
    instructions.push_back(std::move(inst));
    return instructions.back().get();
  }
};

class AsmFunction {
public:
  std::string name;
  std::vector<std::unique_ptr<AsmBlock>> blocks;

  AsmBlock *createBlock(const std::string &label) {
    auto block = std::make_unique<AsmBlock>(label);
    blocks.push_back(std::move(block));
    return blocks.back().get();
  }
};

} // namespace rc::backend
