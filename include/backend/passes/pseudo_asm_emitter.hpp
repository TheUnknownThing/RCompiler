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
  std::string opcode_name(InstOpcode opcode) const;
  std::string operand_name(const std::shared_ptr<AsmOperand> &operand) const;
  std::string inst_name(const AsmInst &inst) const;
};





} // namespace rc::backend
