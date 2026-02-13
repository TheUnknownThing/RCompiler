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
#include <cstdint>
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

private:
  std::vector<std::unique_ptr<AsmFunction>> functions_;

  size_t regID_ = 1;
  size_t uniqueNameID_ = 1; // for generating unique labels
  std::unordered_map<const ir::BasicBlock *, std::string> blockNames_;
  std::unordered_map<const ir::Function *, std::string> functionNames_;
  std::unordered_map<const ir::Value *, std::shared_ptr<AsmOperand>>
      valueOperandMap_;

  AsmBlock *currentBlock_{nullptr};
  size_t stackOffset_ = 0;

  struct TypeLayoutInfo {
    size_t size;
    size_t align;
  };
  TypeLayoutInfo computeTypeLayout(const ir::TypePtr &ty) const;
  size_t computeTypeByteSize(const ir::TypePtr &ty) const;
  size_t alignTo(size_t offset, size_t align) const;

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
  auto asmFunction = std::make_unique<AsmFunction>();
  asmFunction->name = getUniqueFunctionName(&function);
  functions_.push_back(std::move(asmFunction));
  stackOffset_ = 0;
  for (const auto &basicBlock : function.blocks()) {
    visit(*basicBlock);
  }
}

inline void InstructionSelection::visit(const ir::BasicBlock &basicBlock) {
  auto &asmFunction = functions_.back();
  std::string label = getUniqueLabel(&basicBlock);
  auto asmBlock = asmFunction->createBlock(label);
  currentBlock_ = asmBlock;
  for (const auto &inst : basicBlock.instructions()) {
    inst->accept(*this);
    if (inst->isTerminator()) {
      break;
    }
  }
}

inline void InstructionSelection::visit(const ir::BinaryOpInst &binOp) {
  auto dst = createVirtualRegister();
  std::shared_ptr<AsmOperand> lhs = nullptr, rhs = nullptr;
  if (valueOperandMap_.find(binOp.lhs().get()) != valueOperandMap_.end()) {
    lhs = valueOperandMap_[binOp.lhs().get()];
  } else {
    if (auto constInt =
            std::dynamic_pointer_cast<ir::ConstantInt>(binOp.lhs())) {
      lhs = createImmediate(constInt->value());
    } else {
      throw std::runtime_error("Unsupported operand for binary operation");
    }
  }

  if (valueOperandMap_.find(binOp.rhs().get()) != valueOperandMap_.end()) {
    rhs = valueOperandMap_[binOp.rhs().get()];
  } else {
    if (auto constInt =
            std::dynamic_pointer_cast<ir::ConstantInt>(binOp.rhs())) {
      rhs = createImmediate(constInt->value());
    } else {
      throw std::runtime_error("Unsupported operand for binary operation");
    }
  }

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
    throw std::runtime_error(
        "InstructionSelection: unhandled ir::BinaryOpKind");
  }
}

inline void InstructionSelection::visit(const ir::AllocaInst &alloca) {}

inline void InstructionSelection::visit(const ir::LoadInst &load) {
  // TODO
}

inline void InstructionSelection::visit(const ir::StoreInst &store) {
  // TODO
}

inline void InstructionSelection::visit(const ir::GetElementPtrInst &gep) {
  // TODO
}

inline void InstructionSelection::visit(const ir::BranchInst &branch) {
  if (branch.isConditional()) {
    currentBlock_->createInst(InstOpcode::BEQZ, nullptr,
                              {getReg(valueOperandMap_[branch.cond().get()]),
                               createLabelOperand(branch.dest()),
                               createLabelOperand(branch.altDest())});
  } else {
    currentBlock_->createInst(InstOpcode::J, nullptr,
                              {createLabelOperand(branch.dest())});
  }
}

inline void InstructionSelection::visit(const ir::ReturnInst &ret) {
  if (ret.isVoid()) {
    currentBlock_->createInst(InstOpcode::RET, nullptr, {});
  } else {
    currentBlock_->createInst(InstOpcode::RET, nullptr,
                              {getReg(valueOperandMap_[ret.value().get()])});
  }
}

inline void InstructionSelection::visit(const ir::UnreachableInst &) {}

inline void InstructionSelection::visit(const ir::ICmpInst &icmp) {
  auto dst = createVirtualRegister();
  std::shared_ptr<AsmOperand> lhs = nullptr, rhs = nullptr;
  if (valueOperandMap_.find(icmp.lhs().get()) != valueOperandMap_.end()) {
    lhs = valueOperandMap_[icmp.lhs().get()];
  } else {
    if (auto constInt =
            std::dynamic_pointer_cast<ir::ConstantInt>(icmp.lhs())) {
      lhs = createImmediate(constInt->value());
    } else {
      throw std::runtime_error("Unsupported operand for ICmpInst");
    }
  }
  if (valueOperandMap_.find(icmp.rhs().get()) != valueOperandMap_.end()) {
    rhs = valueOperandMap_[icmp.rhs().get()];
  } else {
    if (auto constInt =
            std::dynamic_pointer_cast<ir::ConstantInt>(icmp.rhs())) {
      rhs = createImmediate(constInt->value());
    } else {
      throw std::runtime_error("Unsupported operand for ICmpInst");
    }
  }

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
    throw std::runtime_error("InstructionSelection: unhandled ICmpPred");
  }
}

inline void InstructionSelection::visit(const ir::CallInst &call) {
  // TODO
}

inline void InstructionSelection::visit(const ir::PhiInst &) {
  throw std::runtime_error(
      "InstructionSelection: PhiInst should have been eliminated");
}

inline void InstructionSelection::visit(const ir::SelectInst &) {
  // We do not emit SelectInst previously, so leave blank
}

inline void InstructionSelection::visit(const ir::ZExtInst &zextInst) {
  auto dst = createVirtualRegister();
  std::shared_ptr<AsmOperand> src = nullptr;
  if (valueOperandMap_.find(zextInst.source().get()) !=
      valueOperandMap_.end()) {
    src = valueOperandMap_[zextInst.source().get()];
  } else {
    if (auto constInt =
            std::dynamic_pointer_cast<ir::ConstantInt>(zextInst.source())) {
      src = createImmediate(constInt->value());
    } else {
      throw std::runtime_error("Unsupported operand for ZExtInst");
    }
  }

  valueOperandMap_[&zextInst] = dst;
  currentBlock_->createInst(InstOpcode::ANDI, dst,
                            {getReg(src), createImmediate(0xFFFFFFFF)});
}

inline void InstructionSelection::visit(const ir::SExtInst &sextInst) {
  auto dst = createVirtualRegister();
  std::shared_ptr<AsmOperand> src = nullptr;
  if (valueOperandMap_.find(sextInst.source().get()) !=
      valueOperandMap_.end()) {
    src = valueOperandMap_[sextInst.source().get()];
  } else {
    if (auto constInt =
            std::dynamic_pointer_cast<ir::ConstantInt>(sextInst.source())) {
      src = createImmediate(constInt->value());
    } else {
      throw std::runtime_error("Unsupported operand for SExtInst");
    }
  }

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
  std::shared_ptr<AsmOperand> src = nullptr;
  if (valueOperandMap_.find(truncInst.source().get()) !=
      valueOperandMap_.end()) {
    src = valueOperandMap_[truncInst.source().get()];
  } else {
    if (auto constInt =
            std::dynamic_pointer_cast<ir::ConstantInt>(truncInst.source())) {
      src = createImmediate(constInt->value());
    } else {
      throw std::runtime_error("Unsupported operand for TruncInst");
    }
  }

  valueOperandMap_[&truncInst] = dst;
  currentBlock_->createInst(
      InstOpcode::ANDI, dst,
      {getReg(src), createImmediate((1u << truncInst.destBits()) - 1)});
}

inline void InstructionSelection::visit(const ir::MoveInst &moveInst) {
  // addi rd, rs, 0
  auto dst = createVirtualRegister();
  std::shared_ptr<AsmOperand> src = nullptr;
  if (valueOperandMap_.find(moveInst.source().get()) !=
      valueOperandMap_.end()) {
    src = valueOperandMap_[moveInst.source().get()];
  } else {
    if (auto constInt =
            std::dynamic_pointer_cast<ir::ConstantInt>(moveInst.source())) {
      src = createImmediate(constInt->value());
    } else {
      throw std::runtime_error("Unsupported operand for MoveInst");
    }
  }
  valueOperandMap_[&moveInst] = dst;
  currentBlock_->createInst(InstOpcode::ADDI, dst,
                            {getReg(src), createImmediate(0)});
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