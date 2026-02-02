#pragma once

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

namespace rc::opt {
class IRBaseVisitor {
public:
  virtual ~IRBaseVisitor() = default;

  virtual void visit(ir::Value &value) = 0;

  virtual void visit(ir::Module &module);
  virtual void visit(ir::Function &function);
  virtual void visit(ir::BasicBlock &basicBlock);

  virtual void visit(ir::BinaryOpInst &binaryOpInst);
  virtual void visit(ir::BranchInst &branchInst);
  virtual void visit(ir::UnreachableInst &unreachableInst);
  virtual void visit(ir::ReturnInst &returnInst);
  virtual void visit(ir::AllocaInst &allocaInst);
  virtual void visit(ir::LoadInst &loadInst);
  virtual void visit(ir::StoreInst &storeInst);
  virtual void visit(ir::GetElementPtrInst &getElementPtrInst);
  virtual void visit(ir::ICmpInst &icmpInst);
  virtual void visit(ir::SExtInst &sextInst);
  virtual void visit(ir::ZExtInst &zextInst);
  virtual void visit(ir::TruncInst &truncInst);
  virtual void visit(ir::CallInst &callInst);
  virtual void visit(ir::PhiInst &phiInst);
  virtual void visit(ir::SelectInst &selectInst);
};

inline void IRBaseVisitor::visit(ir::Module &) {}
inline void IRBaseVisitor::visit(ir::Function &) {}
inline void IRBaseVisitor::visit(ir::BasicBlock &) {}

inline void IRBaseVisitor::visit(ir::BinaryOpInst &) {}
inline void IRBaseVisitor::visit(ir::BranchInst &) {}
inline void IRBaseVisitor::visit(ir::UnreachableInst &) {}
inline void IRBaseVisitor::visit(ir::ReturnInst &) {}
inline void IRBaseVisitor::visit(ir::AllocaInst &) {}
inline void IRBaseVisitor::visit(ir::LoadInst &) {}
inline void IRBaseVisitor::visit(ir::StoreInst &) {}
inline void IRBaseVisitor::visit(ir::GetElementPtrInst &) {}
inline void IRBaseVisitor::visit(ir::ICmpInst &) {}
inline void IRBaseVisitor::visit(ir::SExtInst &) {}
inline void IRBaseVisitor::visit(ir::ZExtInst &) {}
inline void IRBaseVisitor::visit(ir::TruncInst &) {}
inline void IRBaseVisitor::visit(ir::CallInst &) {}
inline void IRBaseVisitor::visit(ir::PhiInst &) {}
inline void IRBaseVisitor::visit(ir::SelectInst &) {}
} // namespace rc::opt