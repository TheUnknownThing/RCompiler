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
  mutable size_t long_branch_id_{0};

  std::string opcode_name(InstOpcode opcode) const;
  std::string reg_name(const std::shared_ptr<AsmOperand> &operand) const;
  std::string symbol_name(const std::shared_ptr<AsmOperand> &operand) const;
  int32_t immediate_value(const std::shared_ptr<AsmOperand> &operand) const;
  size_t stack_offset(const AsmFunction &function,
                     const std::shared_ptr<AsmOperand> &operand) const;
  bool fits_signed12(int64_t value) const;
  void emit_load(const AsmFunction &function, std::ostream &os,
                const std::string &mnemonic,
                const std::shared_ptr<AsmOperand> &dst,
                const std::shared_ptr<AsmOperand> &slot_or_base,
                const std::shared_ptr<AsmOperand> &maybe_offset) const;
  void emit_store(const AsmFunction &function, std::ostream &os,
                 const std::string &mnemonic,
                 const std::shared_ptr<AsmOperand> &src,
                 const std::shared_ptr<AsmOperand> &slot_or_base,
                 const std::shared_ptr<AsmOperand> &maybe_offset) const;
  void emit_inst(const AsmFunction &function, const AsmInst &inst,
                std::ostream &os) const;
};

} // namespace rc::backend
