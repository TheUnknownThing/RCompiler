#include "opt/base/base_visitor.hpp"

namespace rc::opt {

void IRBaseVisitor::visit(ir::Module &) {}
void IRBaseVisitor::visit(ir::Function &) {}
void IRBaseVisitor::visit(ir::BasicBlock &) {}
void IRBaseVisitor::visit(ir::BinaryOpInst &) {}
void IRBaseVisitor::visit(ir::BranchInst &) {}
void IRBaseVisitor::visit(ir::UnreachableInst &) {}
void IRBaseVisitor::visit(ir::ReturnInst &) {}
void IRBaseVisitor::visit(ir::AllocaInst &) {}
void IRBaseVisitor::visit(ir::LoadInst &) {}
void IRBaseVisitor::visit(ir::StoreInst &) {}
void IRBaseVisitor::visit(ir::GetElementPtrInst &) {}
void IRBaseVisitor::visit(ir::ICmpInst &) {}
void IRBaseVisitor::visit(ir::SExtInst &) {}
void IRBaseVisitor::visit(ir::ZExtInst &) {}
void IRBaseVisitor::visit(ir::TruncInst &) {}
void IRBaseVisitor::visit(ir::CallInst &) {}
void IRBaseVisitor::visit(ir::PhiInst &) {}
void IRBaseVisitor::visit(ir::SelectInst &) {}

} // namespace rc::opt
