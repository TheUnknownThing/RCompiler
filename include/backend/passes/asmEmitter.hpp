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











} // namespace rc::backend
