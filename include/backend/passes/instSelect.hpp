#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "ir/instructions/visitor.hpp"

#include "backend/nodes/instructions.hpp"
#include "backend/nodes/operands.hpp"

#include "utils/logger.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace rc::backend {
class InstructionSelection : public ir::InstructionVisitor {
public:
  void generate(const ir::Module &module);
  void visit(const ir::Function &function);
  void visit(const ir::BasicBlock &basicBlock);

  void visit(const ir::BinaryOpInst &) override;
  void visit(const ir::AllocaInst &) override;
  void visit(const ir::LoadInst &) override;
  void visit(const ir::StoreInst &) override;
  void visit(const ir::GetElementPtrInst &) override;

  void visit(const ir::BranchInst &) override;
  void visit(const ir::ReturnInst &) override;
  void visit(const ir::UnreachableInst &) override;

  void visit(const ir::ICmpInst &) override;
  void visit(const ir::CallInst &) override;
  void visit(const ir::PhiInst &) override;
  void visit(const ir::SelectInst &) override;
  void visit(const ir::ZExtInst &) override;
  void visit(const ir::SExtInst &) override;
  void visit(const ir::TruncInst &) override;
  void visit(const ir::MoveInst &) override;

  const std::vector<std::unique_ptr<AsmFunction>> &functions() const {
    return functions_;
  }

private:
  std::vector<std::unique_ptr<AsmFunction>> functions_;

  size_t regID_ = 1;
  size_t uniqueNameID_ = 1; // for generating unique labels
  std::unordered_map<const ir::BasicBlock *, std::string> blockNames_;
  std::unordered_map<const ir::Function *, std::string> functionNames_;
  std::unordered_map<const ir::Value *, std::shared_ptr<AsmOperand>>
      valueOperandMap_;

  const ir::Function *currentFunction_{nullptr};
  const ir::BasicBlock *currentIrBlock_{nullptr};

  AsmBlock *currentBlock_{nullptr};
  size_t stackOffset_ = 0;
  const ir::BasicBlock *entryIrBlock_{nullptr};

  std::string describe(const ir::Value *value) const;
  std::shared_ptr<AsmOperand>
  resolveOperandOrImmediate(const std::shared_ptr<ir::Value> &value,
                            const char *reason,
                            const ir::Instruction *inst = nullptr);

  bool isAggregateType(const ir::TypePtr &ty) const;

  struct TypeLayoutInfo {
    size_t size;
    size_t align;
  };
  TypeLayoutInfo computeTypeLayout(const ir::TypePtr &ty) const;
  size_t computeTypeByteSize(const ir::TypePtr &ty) const;
  size_t alignTo(size_t offset, size_t align) const;
  std::shared_ptr<Register>
  materializeAddress(const std::shared_ptr<AsmOperand> &ptr,
                     const ir::Instruction *inst, const char *reason);
  void emitAddImmediate(const std::shared_ptr<Register> &dst,
                        const std::shared_ptr<AsmOperand> &lhs,
                        int64_t immValue);
  std::shared_ptr<AsmOperand>
  emitScaledIndex(const std::shared_ptr<ir::Value> &index, size_t stride,
                  const ir::Instruction *inst);

  std::string getUniqueLabel(const ir::BasicBlock *block) {
    if (blockNames_.count(block) == 0) {
      blockNames_[block] = block->parent()->name() + "." + block->name() + "." +
                           std::to_string(uniqueNameID_++);
    }
    return blockNames_[block];
  }

  std::string getUniqueFunctionName(const ir::Function *function) {
    if (functionNames_.count(function) == 0) {
      functionNames_[function] = function->name();
    }
    return functionNames_[function];
  }

  std::shared_ptr<Register> createVirtualRegister();
  std::shared_ptr<Register> getOrCreateResultRegister(const ir::Value *value);
  std::shared_ptr<Register> createPhysicalRegister(int id);
  std::shared_ptr<Immediate> createImmediate(int32_t value);
  std::shared_ptr<StackSlot> createStackSlot(const ir::TypePtr &type);
  std::shared_ptr<StackSlot> createIncomingArgSlot(size_t argIndex);
  std::shared_ptr<Symbol> createLabelOperand(const ir::BasicBlock *block);
  std::shared_ptr<Symbol> createFunctionOperand(const ir::Function *function);
  Immediate *asImmediate(const std::shared_ptr<AsmOperand> &op);
  std::shared_ptr<AsmOperand> getReg(const std::shared_ptr<AsmOperand> &op);
  std::shared_ptr<AsmOperand>
  prepareCallArgument(const std::shared_ptr<ir::Value> &arg,
                      const ir::CallInst &call);
};








































} // namespace rc::backend
