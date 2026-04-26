#pragma once

#include <cstdint>
#include <string>

namespace rc::backend {

enum class OperandType { REG, IMM, SYMBOL, STACK_SLOT };
enum class StackSlotKind { FRAME, INCOMING_ARG };

class AsmOperand {
public:
  virtual ~AsmOperand() = default;
  OperandType type;
};

class Register : public AsmOperand {
public:
  explicit Register() : id(0), is_virtual(true), spilled(false) { type = OperandType::REG; }

  size_t id;
  bool is_virtual;
  bool spilled = false;
};

class Immediate : public AsmOperand {
public:
  explicit Immediate(int32_t val) : value(val) { type = OperandType::IMM; }

  int32_t value;
  bool is_valid_12() { return value >= -2048 && value <= 2047; }
  bool is_valid_13() { return value >= -4096 && value <= 4095; }
};

class StackSlot : public AsmOperand {
public:
  explicit StackSlot(size_t offset, size_t size,
                     StackSlotKind kind = StackSlotKind::FRAME)
      : offset(offset), size(size), kind(kind) {
    type = OperandType::STACK_SLOT;
  }

  size_t offset;
  size_t size;
  StackSlotKind kind;
};

class Symbol : public AsmOperand {
public:
  explicit Symbol(std::string n, bool is_fn = false)
      : name(std::move(n)), is_function(is_fn) {
    type = OperandType::SYMBOL;
  }

  std::string name;
  bool is_function;
};

} // namespace rc::backend
