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

#include <cstdint>
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
  std::pair<size_t, size_t>
  resolveGEP(const ir::GetElementPtrInst &gep) const; // <offset, size>

  std::string getUniqueLabel(const ir::BasicBlock *block) {
    if (blockNames_.count(block) == 0) {
      blockNames_[block] = block->parent()->name() + "." + block->name() + "." +
                           std::to_string(uniqueNameID_++);
    }
    return blockNames_[block];
  }

  std::string getUniqueFunctionName(const ir::Function *function) {
    if (functionNames_.count(function) == 0) {
      functionNames_[function] =
          function->name() + "." + std::to_string(uniqueNameID_++);
    }
    return functionNames_[function];
  }

  std::shared_ptr<Register> createVirtualRegister();
  std::shared_ptr<Register> createPhysicalRegister(int id);
  std::shared_ptr<Immediate> createImmediate(int32_t value);
  std::shared_ptr<StackSlot> createStackSlot(const ir::TypePtr &type);
  std::shared_ptr<Symbol> createLabelOperand(const ir::BasicBlock *block);
  std::shared_ptr<Symbol> createFunctionOperand(const ir::Function *function);
  Immediate *asImmediate(const std::shared_ptr<AsmOperand> &op);
  std::shared_ptr<AsmOperand> getReg(const std::shared_ptr<AsmOperand> &op);
};

inline void InstructionSelection::generate(const ir::Module &module) {
  for (const auto &function : module.functions()) {
    visit(*function);
  }
}

inline void InstructionSelection::visit(const ir::Function &function) {
  currentFunction_ = &function;
  LOG_DEBUG("[ISel] Begin function: " + function.name());

  auto asmFunction = std::make_unique<AsmFunction>();
  asmFunction->name = getUniqueFunctionName(&function);
  functions_.push_back(std::move(asmFunction));

  // add arguments to valueOperandMap_
  if (function.args().size() > 0) {
    for (size_t i = 0; i <= 7 && i < function.args().size(); ++i) {
      valueOperandMap_[function.args()[i].get()] =
          createPhysicalRegister(10 + i);
    }
    for (size_t i = 8; i < function.args().size(); ++i) {
      valueOperandMap_[function.args()[i].get()] =
          createStackSlot(function.args()[i]->type());
    }
  }

  stackOffset_ = 0;
  if (!function.blocks().empty()) {
    for (const auto &basicBlock : function.blocks()) {
      visit(*basicBlock);
    }
  }
  LOG_DEBUG("[ISel] End function: " + function.name());
  functions_.back()->stackSize = alignTo(stackOffset_, 16);
  currentFunction_ = nullptr;
}

inline void InstructionSelection::visit(const ir::BasicBlock &basicBlock) {
  currentIrBlock_ = &basicBlock;

  auto &asmFunction = functions_.back();
  std::string label = getUniqueLabel(&basicBlock);
  LOG_DEBUG("[ISel] Enter block: " + label + " (ir=" + basicBlock.name() + ")");

  auto asmBlock = asmFunction->createBlock(label);
  currentBlock_ = asmBlock;
  for (const auto &inst : basicBlock.instructions()) {
    if (!inst) {
      throw std::runtime_error(
          "InstructionSelection: null instruction in block " + label);
    }

    LOG_DEBUG("[ISel] Select inst name=" +
              (inst->name().empty() ? std::string("<unnamed>") : inst->name()));
    inst->accept(*this);
    if (inst->isTerminator()) {
      break;
    }
  }

  LOG_DEBUG("[ISel] Exit block: " + label);
  currentIrBlock_ = nullptr;
}

inline void InstructionSelection::visit(const ir::BinaryOpInst &binOp) {
  auto dst = createVirtualRegister();
  auto lhs = resolveOperandOrImmediate(
      binOp.lhs(), "LHS operand for binary operation", &binOp);
  auto rhs = resolveOperandOrImmediate(
      binOp.rhs(), "RHS operand for binary operation", &binOp);

  valueOperandMap_[&binOp] = dst;

  auto immFitsShamt = [](int32_t v) -> bool { return v >= 0 && v < 32; };

  switch (binOp.op()) {
  case rc::ir::BinaryOpKind::ADD: {
    auto *lhsImm = asImmediate(lhs);
    auto *rhsImm = asImmediate(rhs);
    if (!lhsImm && rhsImm && rhsImm->is_valid_12()) {
      currentBlock_->createInst(InstOpcode::ADDI, dst, {lhs, rhs});
      break;
    }
    if (!rhsImm && lhsImm && lhsImm->is_valid_12()) {
      currentBlock_->createInst(InstOpcode::ADDI, dst, {rhs, lhs});
      break;
    }
    currentBlock_->createInst(InstOpcode::ADD, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::SUB: {
    if (auto *imm = asImmediate(rhs); imm) {
      int32_t neg64 = -static_cast<int32_t>(imm->value);
      if (neg64 >= -2048 && neg64 <= 2047) {
        auto negImm = createImmediate(static_cast<int32_t>(neg64));
        currentBlock_->createInst(InstOpcode::ADDI, dst, {getReg(lhs), negImm});
        break;
      }
    }
    currentBlock_->createInst(InstOpcode::SUB, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::MUL: {
    currentBlock_->createInst(InstOpcode::MUL, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::SDIV: {
    currentBlock_->createInst(InstOpcode::DIV, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::UDIV: {
    currentBlock_->createInst(InstOpcode::DIVU, dst,
                              {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::SREM: {
    currentBlock_->createInst(InstOpcode::REM, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::UREM: {
    currentBlock_->createInst(InstOpcode::REMU, dst,
                              {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::SHL: {
    if (auto *imm = asImmediate(rhs); imm && immFitsShamt(imm->value)) {
      currentBlock_->createInst(InstOpcode::SLLI, dst, {getReg(lhs), rhs});
      break;
    }
    currentBlock_->createInst(InstOpcode::SLL, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::ASHR: {
    if (auto *imm = asImmediate(rhs); imm && immFitsShamt(imm->value)) {
      currentBlock_->createInst(InstOpcode::SRAI, dst, {getReg(lhs), rhs});
      break;
    }
    currentBlock_->createInst(InstOpcode::SRA, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::LSHR: {
    if (auto *imm = asImmediate(rhs); imm && immFitsShamt(imm->value)) {
      currentBlock_->createInst(InstOpcode::SRLI, dst, {getReg(lhs), rhs});
      break;
    }
    currentBlock_->createInst(InstOpcode::SRL, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::AND: {
    if (auto *imm = asImmediate(rhs);
        imm && imm->is_valid_12() && !asImmediate(lhs)) {
      currentBlock_->createInst(InstOpcode::ANDI, dst, {lhs, rhs});
      break;
    }
    if (auto *imm = asImmediate(lhs);
        imm && imm->is_valid_12() && !asImmediate(rhs)) {
      currentBlock_->createInst(InstOpcode::ANDI, dst, {rhs, lhs});
      break;
    }
    currentBlock_->createInst(InstOpcode::AND, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::OR: {
    if (auto *imm = asImmediate(rhs);
        imm && imm->is_valid_12() && !asImmediate(lhs)) {
      currentBlock_->createInst(InstOpcode::ORI, dst, {lhs, rhs});
      break;
    }
    if (auto *imm = asImmediate(lhs);
        imm && imm->is_valid_12() && !asImmediate(rhs)) {
      currentBlock_->createInst(InstOpcode::ORI, dst, {rhs, lhs});
      break;
    }
    currentBlock_->createInst(InstOpcode::OR, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  case rc::ir::BinaryOpKind::XOR: {
    if (auto *imm = asImmediate(rhs);
        imm && imm->is_valid_12() && !asImmediate(lhs)) {
      currentBlock_->createInst(InstOpcode::XORI, dst, {lhs, rhs});
      break;
    }
    if (auto *imm = asImmediate(lhs);
        imm && imm->is_valid_12() && !asImmediate(rhs)) {
      currentBlock_->createInst(InstOpcode::XORI, dst, {rhs, lhs});
      break;
    }
    currentBlock_->createInst(InstOpcode::XOR, dst, {getReg(lhs), getReg(rhs)});
    break;
  }
  default:
    throw std::runtime_error("unhandled binary operation");
  }
}

inline void InstructionSelection::visit(const ir::AllocaInst &alloca) {
  auto dst = createVirtualRegister();
  valueOperandMap_[&alloca] = dst;

  createStackSlot(alloca.allocatedType());
}

inline void InstructionSelection::visit(const ir::LoadInst &load) {
  if (isAggregateType(load.type())) {
    throw std::runtime_error("loading aggregate should not exist");
  }
  auto dst = createVirtualRegister();
  auto slot = std::dynamic_pointer_cast<StackSlot>(
      valueOperandMap_[load.pointer().get()]);

  if (!slot) {
    throw std::runtime_error("LoadInst operand is not a stack slot");
  }
  valueOperandMap_[&load] = dst;

  size_t loadSize = computeTypeByteSize(load.type());
  if (loadSize == 0) {
    throw std::runtime_error("cannot load type with zero byte size");
  } else if (loadSize == 1) {
    currentBlock_->createInst(InstOpcode::LB, dst, {slot});
  } else if (loadSize == 2) {
    currentBlock_->createInst(InstOpcode::LH, dst, {slot});
  } else if (loadSize == 4) {
    currentBlock_->createInst(InstOpcode::LW, dst, {slot});
  } else {
    throw std::runtime_error("unsupported load size: " +
                             std::to_string(loadSize));
  }
}

inline void InstructionSelection::visit(const ir::StoreInst &store) {
  if (isAggregateType(store.type())) {
    throw std::runtime_error("storing aggregate should not exist");
  }

  auto src = resolveOperandOrImmediate(store.value(), "store value", &store);
  auto slot = std::dynamic_pointer_cast<StackSlot>(
      valueOperandMap_[store.pointer().get()]);
  if (!slot) {
    throw std::runtime_error("StoreInst pointer operand is not a stack slot");
  }

  size_t storeSize = computeTypeByteSize(store.value()->type());
  if (storeSize == 0) {
    throw std::runtime_error("cannot store type with zero byte size");
  } else if (storeSize == 1) {
    currentBlock_->createInst(InstOpcode::SB, nullptr, {getReg(src), slot});
  } else if (storeSize == 2) {
    currentBlock_->createInst(InstOpcode::SH, nullptr, {getReg(src), slot});
  } else if (storeSize == 4) {
    currentBlock_->createInst(InstOpcode::SW, nullptr, {getReg(src), slot});
  } else {
    throw std::runtime_error("unsupported store size: " +
                             std::to_string(storeSize));
  }
}

inline void InstructionSelection::visit(const ir::GetElementPtrInst &gep) {
  auto [offset, size] = resolveGEP(gep);
  if (size == 0) {
    throw std::runtime_error("cannot compute GEP with zero-size type");
  }

  auto slot = std::dynamic_pointer_cast<StackSlot>(
      valueOperandMap_[gep.basePointer().get()]);

  if (!slot) {
    throw std::runtime_error("GEP base pointer is not a stack slot");
  }

  auto newSlot = std::make_shared<StackSlot>(slot->offset + offset, size);
  valueOperandMap_[&gep] = newSlot;
}

inline void InstructionSelection::visit(const ir::BranchInst &branch) {
  if (branch.isConditional()) {
    auto cond = resolveOperandOrImmediate(
        branch.cond(), "conditional branch condition not materialized",
        &branch);
    currentBlock_->createInst(InstOpcode::BNEZ, nullptr,
                              {getReg(cond), createLabelOperand(branch.dest()),
                               createLabelOperand(branch.altDest())});
  } else {
    if (!branch.dest()) {
      throw std::runtime_error("unconditional branch with null destination");
    }
    currentBlock_->createInst(InstOpcode::J, nullptr,
                              {createLabelOperand(branch.dest())});
  }
}

inline void InstructionSelection::visit(const ir::ReturnInst &ret) {
  if (ret.isVoid()) {
    currentBlock_->createInst(InstOpcode::RET, nullptr, {});
  } else {
    auto retVal = resolveOperandOrImmediate(
        ret.value(), "return value not materialized", &ret);
    currentBlock_->createInst(InstOpcode::RET, nullptr, {getReg(retVal)});
  }
}

inline void InstructionSelection::visit(const ir::UnreachableInst &) {}

inline void InstructionSelection::visit(const ir::ICmpInst &icmp) {
  auto dst = createVirtualRegister();
  auto lhs =
      resolveOperandOrImmediate(icmp.lhs(), "LHS operand for ICmpInst", &icmp);
  auto rhs =
      resolveOperandOrImmediate(icmp.rhs(), "RHS operand for ICmpInst", &icmp);

  valueOperandMap_[&icmp] = dst;
  switch (icmp.pred()) {
  case ir::ICmpPred::EQ:
    // rd = (rs1 ^ rs2) == 0
    currentBlock_->createInst(InstOpcode::XOR, dst, {getReg(lhs), getReg(rhs)});
    currentBlock_->createInst(InstOpcode::SLTIU, dst,
                              {getReg(dst), createImmediate(1)});
    break;
  case ir::ICmpPred::NE:
    // rd = (rs1 ^ rs2) != 0
    currentBlock_->createInst(InstOpcode::XOR, dst, {getReg(lhs), getReg(rhs)});
    currentBlock_->createInst(InstOpcode::SLTU, dst,
                              {createImmediate(0), getReg(dst)});
    break;
  case ir::ICmpPred::UGT:
    currentBlock_->createInst(InstOpcode::SLTU, dst,
                              {getReg(rhs), getReg(lhs)});
    break;
  case ir::ICmpPred::SGT:
    currentBlock_->createInst(InstOpcode::SLT, dst, {getReg(rhs), getReg(lhs)});
    break;
  case ir::ICmpPred::ULT:
    currentBlock_->createInst(InstOpcode::SLTU, dst,
                              {getReg(lhs), getReg(rhs)});
    break;
  case ir::ICmpPred::SLT:
    currentBlock_->createInst(InstOpcode::SLT, dst, {getReg(lhs), getReg(rhs)});
    break;
  case ir::ICmpPred::UGE:
    currentBlock_->createInst(InstOpcode::SLTU, dst,
                              {getReg(lhs), getReg(rhs)});
    currentBlock_->createInst(InstOpcode::XORI, dst,
                              {getReg(dst), createImmediate(1)});
    break;
  case ir::ICmpPred::SGE:
    currentBlock_->createInst(InstOpcode::SLT, dst, {getReg(lhs), getReg(rhs)});
    currentBlock_->createInst(InstOpcode::XORI, dst,
                              {getReg(dst), createImmediate(1)});
    break;
  case ir::ICmpPred::ULE:
    currentBlock_->createInst(InstOpcode::SLTU, dst,
                              {getReg(rhs), getReg(lhs)});
    currentBlock_->createInst(InstOpcode::XORI, dst,
                              {getReg(dst), createImmediate(1)});
    break;
  case ir::ICmpPred::SLE:
    currentBlock_->createInst(InstOpcode::SLT, dst, {getReg(rhs), getReg(lhs)});
    currentBlock_->createInst(InstOpcode::XORI, dst,
                              {getReg(dst), createImmediate(1)});
    break;
  default:
    throw std::runtime_error("unknown ICmp predicate");
  }
}

inline void InstructionSelection::visit(const ir::CallInst &call) {
  if (call.type()->isVoid()) {
    std::vector<std::shared_ptr<AsmOperand>> args;
    for (const auto &arg : call.args()) {
      args.push_back(resolveOperandOrImmediate(
          arg, "call argument not materialized", &call));
    }

    std::vector<std::shared_ptr<AsmOperand>> operands;
    operands.push_back(createFunctionOperand(call.calleeFunction()));
    operands.insert(operands.end(), args.begin(), args.end());
    currentBlock_->createInst(InstOpcode::CALL, nullptr, operands);
  } else {
    auto dst = createVirtualRegister();
    valueOperandMap_[&call] = dst;
    std::vector<std::shared_ptr<AsmOperand>> args;
    for (const auto &arg : call.args()) {
      args.push_back(resolveOperandOrImmediate(
          arg, "call argument not materialized", &call));
    }

    std::vector<std::shared_ptr<AsmOperand>> operands;
    operands.push_back(createFunctionOperand(call.calleeFunction()));
    operands.insert(operands.end(), args.begin(), args.end());
    currentBlock_->createInst(InstOpcode::CALL, dst, operands);
  }
}

inline void InstructionSelection::visit(const ir::PhiInst &) {
  throw std::runtime_error("PhiInst should have been removed by now");
}

inline void InstructionSelection::visit(const ir::SelectInst &) {
  // We do not emit SelectInst previously, so leave blank
}

inline void InstructionSelection::visit(const ir::ZExtInst &zextInst) {
  auto dst = createVirtualRegister();
  auto src = resolveOperandOrImmediate(zextInst.source(),
                                       "operand for ZExtInst", &zextInst);

  valueOperandMap_[&zextInst] = dst;
  currentBlock_->createInst(InstOpcode::ANDI, dst,
                            {getReg(src), createImmediate(0xFFFFFFFF)});
}

inline void InstructionSelection::visit(const ir::SExtInst &sextInst) {
  auto dst = createVirtualRegister();
  auto src = resolveOperandOrImmediate(sextInst.source(),
                                       "operand for SExtInst", &sextInst);

  valueOperandMap_[&sextInst] = dst;
  currentBlock_->createInst(
      InstOpcode::SLLI, dst,
      {getReg(src), createImmediate(32 - sextInst.srcBits())});
  currentBlock_->createInst(
      InstOpcode::SRAI, dst,
      {getReg(dst), createImmediate(32 - sextInst.srcBits())});
}

inline void InstructionSelection::visit(const ir::TruncInst &truncInst) {
  auto dst = createVirtualRegister();
  auto src = resolveOperandOrImmediate(truncInst.source(),
                                       "operand for TruncInst", &truncInst);

  valueOperandMap_[&truncInst] = dst;
  currentBlock_->createInst(
      InstOpcode::ANDI, dst,
      {getReg(src), createImmediate((1u << truncInst.destBits()) - 1)});
}

inline void InstructionSelection::visit(const ir::MoveInst &moveInst) {
  // addi rd, rs, 0
  std::shared_ptr<AsmOperand> dst = nullptr;
  if (valueOperandMap_.find(moveInst.destination().get()) !=
      valueOperandMap_.end()) {
    dst = valueOperandMap_[moveInst.destination().get()];
  } else {
    dst = createVirtualRegister();
    valueOperandMap_[moveInst.destination().get()] = dst;
  }

  auto src = resolveOperandOrImmediate(moveInst.source(),
                                       "operand for MoveInst", &moveInst);
  currentBlock_->createInst(InstOpcode::ADDI, dst,
                            {getReg(src), createImmediate(0)});
}

inline std::string
InstructionSelection::describe(const ir::Value *value) const {
  if (!value) {
    return "<null value>";
  }

  std::ostringstream oss;
  oss << "name=" << (value->name().empty() ? "<unnamed>" : value->name())
      << ", ptr=" << value
      << ", kind=" << static_cast<int>(value->type()->kind());
  return oss.str();
}

inline std::shared_ptr<AsmOperand>
InstructionSelection::resolveOperandOrImmediate(
    const std::shared_ptr<ir::Value> &value, const char *reason,
    const ir::Instruction *inst) {
  if (!value) {
    throw std::runtime_error(std::string(reason) + ": null value");
  }

  auto it = valueOperandMap_.find(value.get());
  if (it != valueOperandMap_.end() && it->second) {
    return it->second;
  }

  if (auto constInt = std::dynamic_pointer_cast<ir::ConstantInt>(value)) {
    return createImmediate(constInt->value());
  }

  if (std::dynamic_pointer_cast<ir::UndefValue>(value)) {
    LOG_WARN("[ISel] materializing undef as 0");
    return createImmediate(0);
  }

  if (std::dynamic_pointer_cast<ir::Argument>(value)) {
    throw std::runtime_error(std::string(reason) +
                             " is a function argument that has not been mapped "
                             "to a register or stack slot: " +
                             describe(value.get()));
  }

  if (std::dynamic_pointer_cast<ir::Instruction>(value)) {
    throw std::runtime_error(
        std::string(reason) +
        " is an instruction that has not been selected yet: " +
        describe(value.get()));
  }

  throw std::runtime_error(std::string(reason) + ": " + describe(value.get()));
}

inline std::shared_ptr<Register> InstructionSelection::createVirtualRegister() {
  auto reg = std::make_shared<Register>();
  reg->id = regID_++;
  reg->is_virtual = true;
  return reg;
}

inline std::shared_ptr<Register>
InstructionSelection::createPhysicalRegister(int id) {
  auto reg = std::make_shared<Register>();
  reg->id = id;
  reg->is_virtual = false;
  return reg;
}

inline std::shared_ptr<Immediate>
InstructionSelection::createImmediate(int32_t value) {
  return std::make_shared<Immediate>(value);
}

inline std::shared_ptr<StackSlot>
InstructionSelection::createStackSlot(const ir::TypePtr &type) {
  auto layout = computeTypeLayout(type);
  stackOffset_ = alignTo(stackOffset_, layout.align);
  auto slot = std::make_shared<StackSlot>(stackOffset_, layout.size);
  stackOffset_ += layout.size;
  return slot;
}

inline std::shared_ptr<Symbol>
InstructionSelection::createLabelOperand(const ir::BasicBlock *block) {
  return std::make_shared<Symbol>(getUniqueLabel(block), false);
}

inline std::shared_ptr<Symbol>
InstructionSelection::createFunctionOperand(const ir::Function *function) {
  return std::make_shared<Symbol>(getUniqueFunctionName(function), true);
}

inline bool InstructionSelection::isAggregateType(const ir::TypePtr &ty) const {
  return ty->kind() == ir::TypeKind::Array ||
         ty->kind() == ir::TypeKind::Struct;
}

inline size_t InstructionSelection::alignTo(size_t offset, size_t align) const {
  if (align <= 1) {
    return offset;
  }
  auto rem = offset % align;
  return rem ? (offset + (align - rem)) : offset;
}

inline InstructionSelection::TypeLayoutInfo
InstructionSelection::computeTypeLayout(const ir::TypePtr &ty) const {
  if (auto intTy = std::dynamic_pointer_cast<const ir::IntegerType>(ty)) {
    size_t size = (intTy->bits() + 7) / 8;
    if (size == 0) {
      size = 1;
    }
    return {size, size};
  }
  if (auto arrTy = std::dynamic_pointer_cast<const ir::ArrayType>(ty)) {
    auto elem = computeTypeLayout(arrTy->elem());
    size_t stride = alignTo(elem.size, elem.align);
    return {stride * arrTy->count(), elem.align};
  }
  if (auto structTy = std::dynamic_pointer_cast<const ir::StructType>(ty)) {
    size_t offset = 0;
    size_t maxAlign = 1;
    for (const auto &field : structTy->fields()) {
      auto layout = computeTypeLayout(field);
      offset = alignTo(offset, layout.align);
      offset += layout.size;
      maxAlign = std::max(maxAlign, layout.align);
    }
    offset = alignTo(offset, maxAlign);
    return {offset, maxAlign};
  }
  if (std::dynamic_pointer_cast<const ir::PointerType>(ty)) {
    size_t ptrBytes = 4;
    return {ptrBytes, ptrBytes};
  }
  if (ty->isVoid()) {
    return {0, 1};
  }
  throw std::runtime_error("InstructionSelection: unknown type");
}

inline size_t
InstructionSelection::computeTypeByteSize(const ir::TypePtr &ty) const {
  return computeTypeLayout(ty).size;
}

inline std::pair<size_t, size_t>
InstructionSelection::resolveGEP(const ir::GetElementPtrInst &gep) const {
  // TODO
  return {0, 0};
}

inline Immediate *
InstructionSelection::asImmediate(const std::shared_ptr<AsmOperand> &op) {
  if (!op || op->type != OperandType::IMM) {
    return nullptr;
  }
  return static_cast<Immediate *>(op.get());
}

inline std::shared_ptr<AsmOperand>
InstructionSelection::getReg(const std::shared_ptr<AsmOperand> &op) {
  if (!asImmediate(op)) {
    return op;
  }
  auto immReg = createVirtualRegister();
  currentBlock_->createInst(InstOpcode::LI, immReg, {op});
  return immReg;
}

} // namespace rc::backend