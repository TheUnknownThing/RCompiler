#pragma once

#include <cstdint>
#include <string>

namespace rc::backend {

enum class OperandType { REG, IMM, SYMBOL, STACK_SLOT };

class AsmOperand {
public:
  OperandType type;
};

class Register : public AsmOperand {
public:
  int id;
  bool is_virtual;
};

class Immediate : public AsmOperand {
public:
  int32_t value;
  bool is_valid_12() { return value >= -2048 && value <= 2047; }
  bool is_valid_13() { return value >= -4096 && value <= 4095; }
};

class StackSlot : public AsmOperand {
public:
  int offset;
};

class Symbol : public AsmOperand {
public:
  std::string name;
  bool is_function;
};

} // namespace rc::backend