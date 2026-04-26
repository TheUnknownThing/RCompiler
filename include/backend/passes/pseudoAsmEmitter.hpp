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





} // namespace rc::backend
