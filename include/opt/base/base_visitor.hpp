#pragma once

#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"

namespace rc::opt {
class IRBaseVisitor {
public:
  virtual ~IRBaseVisitor() = default;

  virtual void visit(ir::Value &value) = 0;

  virtual void visit(ir::Module &module);
  virtual void visit(ir::Function &function);
  virtual void visit(ir::BasicBlock &basic_block);

  virtual void visit(ir::BinaryOpInst &binary_op_inst);
  virtual void visit(ir::BranchInst &branch_inst);
  virtual void visit(ir::UnreachableInst &unreachable_inst);
  virtual void visit(ir::ReturnInst &return_inst);
  virtual void visit(ir::AllocaInst &alloca_inst);
  virtual void visit(ir::LoadInst &load_inst);
  virtual void visit(ir::StoreInst &store_inst);
  virtual void visit(ir::GetElementPtrInst &get_element_ptr_inst);
  virtual void visit(ir::ICmpInst &icmp_inst);
  virtual void visit(ir::SExtInst &sext_inst);
  virtual void visit(ir::ZExtInst &zext_inst);
  virtual void visit(ir::TruncInst &trunc_inst);
  virtual void visit(ir::CallInst &call_inst);
  virtual void visit(ir::PhiInst &phi_inst);
  virtual void visit(ir::SelectInst &select_inst);
};

} // namespace rc::opt