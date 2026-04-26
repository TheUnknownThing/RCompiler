#pragma once

namespace rc::ir {

class BinaryOpInst;

class AllocaInst;
class LoadInst;
class StoreInst;
class GetElementPtrInst;

class BranchInst;
class ReturnInst;
class UnreachableInst;

class ICmpInst;
class CallInst;
class PhiInst;
class SelectInst;
class ZExtInst;
class SExtInst;
class TruncInst;
class MoveInst;

struct InstructionVisitor {
  virtual ~InstructionVisitor() = default;

  virtual void visit(const BinaryOpInst &) = 0;

  virtual void visit(const AllocaInst &) = 0;
  virtual void visit(const LoadInst &) = 0;
  virtual void visit(const StoreInst &) = 0;
  virtual void visit(const GetElementPtrInst &) = 0;

  virtual void visit(const BranchInst &) = 0;
  virtual void visit(const ReturnInst &) = 0;
  virtual void visit(const UnreachableInst &) = 0;

  virtual void visit(const ICmpInst &) = 0;
  virtual void visit(const CallInst &) = 0;
  virtual void visit(const PhiInst &) = 0;
  virtual void visit(const SelectInst &) = 0;
  virtual void visit(const ZExtInst &) = 0;
  virtual void visit(const SExtInst &) = 0;
  virtual void visit(const TruncInst &) = 0;
  virtual void visit(const MoveInst &) = 0;
};

} // namespace rc::ir
