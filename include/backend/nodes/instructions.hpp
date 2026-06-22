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
  ADDW,
  SUBW,
  SLLW,
  SRLW,
  SRAW,
  MULW,
  DIVW,
  DIVUW,
  REMW,
  REMUW,
  // I-Type
  ADDI,
  ADDIW,
  XORI,
  ORI,
  ANDI,
  SLLI,
  SRLI,
  SRAI,
  SLLIW,
  SRLIW,
  SRAIW,
  SLTI,
  SLTIU,
  LB,
  LH,
  LW,
  LBU,
  LHU,
  LWU,
  LD,
  JALR,
  // S-Type
  SB,
  SH,
  SW,
  SD,
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
  J,
  LA, // load address of a symbol (pseudo: la rd, sym)
  JR  // indirect jump (pseudo: jr rs == jalr x0, 0(rs))
};

class AsmInst {
public:
  AsmInst(InstOpcode opc, std::shared_ptr<AsmOperand> dst,
          std::vector<std::shared_ptr<AsmOperand>> uses)
      : opcode(opc), dst(std::move(dst)), uses(std::move(uses)) {}

  AsmInst(InstOpcode opc, std::vector<std::shared_ptr<AsmOperand>> uses)
      : opcode(opc), dst(nullptr), uses(std::move(uses)) {}

  bool is_pseudo() const {
    switch (opcode) {
    case InstOpcode::MV:
    case InstOpcode::LI:
    case InstOpcode::CALL:
    case InstOpcode::RET:
    case InstOpcode::BEQZ:
    case InstOpcode::BNEZ:
    case InstOpcode::J:
    case InstOpcode::LA:
    case InstOpcode::JR:
      return true;
    default:
      return false;
    }
  }

  InstOpcode get_opcode() const { return opcode; }
  const std::shared_ptr<AsmOperand> &get_dst() const { return dst; }
  const std::vector<std::shared_ptr<AsmOperand>> &get_uses() const {
    return uses;
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

  AsmInst *create_inst(InstOpcode opc, std::shared_ptr<AsmOperand> dst,
                      std::vector<std::shared_ptr<AsmOperand>> uses) {
    auto inst = std::make_unique<AsmInst>(opc, std::move(dst), std::move(uses));
    instructions.push_back(std::move(inst));
    return instructions.back().get();
  }
};

// A read-only jump table emitted to .rodata for a SwitchInst lowered as an
// indexed jump. `entry_labels` holds one block label per index in [lo, hi];
// holes are filled with the default label.
struct JumpTable {
  std::string label;
  std::vector<std::string> entry_labels;
};

class AsmFunction {
public:
  std::string name;
  std::vector<std::unique_ptr<AsmBlock>> blocks;
  std::vector<JumpTable> jump_tables;
  size_t stack_size{0};

  AsmBlock *create_block(const std::string &label) {
    auto block = std::make_unique<AsmBlock>(label);
    blocks.push_back(std::move(block));
    return blocks.back().get();
  }
};

} // namespace rc::backend
