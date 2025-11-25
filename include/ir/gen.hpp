#pragma once

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "instructions/binary.hpp"
#include "instructions/controlFlow.hpp"
#include "instructions/memory.hpp"
#include "instructions/misc.hpp"
#include "instructions/topLevel.hpp"

namespace rc::ir {

class LLVMEmitter {
public:
  explicit LLVMEmitter(std::ostream &out = std::cout) : out_(out) {}

  // Entry points
  void emitModule(const Module &mod) {
    reset();
    collectIdentifiedStructs(mod);
    // Header comments
    out_ << "; ModuleID = '" << mod.name() << "'\n";
    if (!mod.target().dataLayout.empty()) {
      out_ << "target datalayout = \"" << mod.target().dataLayout << "\"\n";
    }
    if (!mod.target().triple.empty()) {
      out_ << "target triple = \"" << mod.target().triple << "\"\n";
    }
    // Identified struct declarations
    for (const auto &decl : structDeclOrder_) {
      out_ << "%" << decl << " = type " << identifiedStructBody_.at(decl)
           << "\n";
    }
    if (!structDeclOrder_.empty())
      out_ << "\n";

    for (const auto &c : mod.constants()) {
      if (!c)
        continue;
      out_ << "@" << constantName(*c) << " = constant "
           << typeToString(c->type()) << " " << valueRef(*c) << "\n";
    }
    if (!mod.constants().empty())
      out_ << "\n";

    // Functions
    for (const auto &fn : mod.functions()) {
      emitFunction(*fn);
      out_ << "\n";
    }
  }

  void emitFunction(const Function &fn) {
    nameState_.clear();
    blockNames_.clear();
    nextValueId_ = 0;
    nextBlockId_ = 0;

    const auto &fty = fn.type();
    const bool isDecl = fn.isExternal();
    std::string mangledName = functionName(fn);

    if (isDecl) {
      out_ << "declare " << typeToString(fty->returnType()) << " @" << mangledName
           << "(";
    } else {
      out_ << "define " << typeToString(fty->returnType()) << " @" << mangledName
           << "(";
    }

    // Arguments
    for (size_t i = 0; i < fn.args().size(); ++i) {
      if (i)
        out_ << ", ";
      const auto &arg = fn.args()[i];
      const auto &pty = fty->paramTypes()[i];
      out_ << typeToString(pty) << " " << localName(arg.get());
    }
    out_ << ")";

    if (isDecl) {
      out_ << "\n";
      return;
    }

    out_ << " {\n";

    // Pre-assign basic block names
    for (const auto &bb : fn.blocks()) {
      (void)localLabel(bb.get());
    }

    for (const auto &bb : fn.blocks()) {
      emitBasicBlock(*bb);
    }

    out_ << "}" << "\n";
  }

private:
  std::ostream &out_;

  // Per-module state for identified structs
  std::unordered_set<std::string> identifiedNames_;
  std::unordered_map<std::string, std::string> identifiedStructBody_;
  std::vector<std::string> structDeclOrder_;

  // Per-function naming state
  std::unordered_map<const Value *, std::string> nameState_;
  std::unordered_map<const BasicBlock *, std::string> blockNames_;
  std::unordered_map<const Function *, std::string> functionNames_;
  std::unordered_map<const StructType *, std::string> structNames_;
  std::unordered_map<const Constant *, std::string> globalConstNames_;
  std::unordered_map<std::string, int> functionNameCounters_;
  std::unordered_map<std::string, int> structNameCounters_;
  std::unordered_map<std::string, int> constantNameCounters_;
  int nextValueId_{0};
  int nextBlockId_{0};

  void reset() {
    identifiedNames_.clear();
    identifiedStructBody_.clear();
    structDeclOrder_.clear();
    functionNames_.clear();
    structNames_.clear();
    globalConstNames_.clear();
    functionNameCounters_.clear();
    structNameCounters_.clear();
    constantNameCounters_.clear();
  }

  // Naming helpers
  std::string localName(const Value *v) {
    auto it = nameState_.find(v);
    if (it != nameState_.end())
      return "%" + it->second;
    std::string base = v->name().empty() ? genTmp() : v->name() + genTmp();
    nameState_[v] = base;
    return "%" + base;
  }

  std::string localLabel(const BasicBlock *bb) {
    auto it = blockNames_.find(bb);
    if (it != blockNames_.end())
      return it->second;
    std::string base = bb->name().empty() ? genBlock() : bb->name() + genBlock();
    blockNames_[bb] = base;
    return base;
  }

  std::string genTmp() { return "val" + std::to_string(nextValueId_++); }
  std::string genBlock() { return "bb" + std::to_string(nextBlockId_++); }
  std::string uniqueGlobal(
      const std::string &base,
      std::unordered_map<std::string, int> &counters) {
    auto &ctr = counters[base];
    std::string name = base;
    if (ctr > 0) {
      name += "." + std::to_string(ctr);
    }
    ++ctr;
    return name;
  }

  std::string functionName(const Function &fn) {
    auto it = functionNames_.find(&fn);
    if (it != functionNames_.end())
      return it->second;
    auto base = fn.name().empty() ? "fn" : fn.name();
    auto mangled = uniqueGlobal(base, functionNameCounters_);
    functionNames_[&fn] = mangled;
    return mangled;
  }

  std::string structName(const StructType &st) {
    auto it = structNames_.find(&st);
    if (it != structNames_.end()) {
      return it->second;
    }
    auto base = st.name().empty() ? "struct" : st.name();
    // auto mangled = uniqueGlobal(base, structNameCounters_);
    structNames_[&st] = base;
    return base;
  }

  std::string constantName(const Constant &c) {
    auto it = globalConstNames_.find(&c);
    if (it != globalConstNames_.end()) {
      return it->second;
    }
    auto base = c.name().empty() ? "cst" : c.name();
    // auto mangled = uniqueGlobal(base, constantNameCounters_);
    globalConstNames_[&c] = base;
    return base;
  }

  // Type printing
  std::string typeToString(const TypePtr &ty) {
    switch (ty->kind()) {
    case TypeKind::Void:
      return "void";
    case TypeKind::Integer: {
      auto it = std::static_pointer_cast<const IntegerType>(ty);
      return "i" + std::to_string(it->bits());
    }
    case TypeKind::Pointer: {
      auto pt = std::static_pointer_cast<const PointerType>(ty);
      return typeToString(pt->pointee()) + "*";
    }
    case TypeKind::Array: {
      auto at = std::static_pointer_cast<const ArrayType>(ty);
      std::ostringstream oss;
      oss << "[" << at->count() << " x " << typeToString(at->elem()) << "]";
      return oss.str();
    }
    case TypeKind::Struct: {
      auto st = std::static_pointer_cast<const StructType>(ty);
      if (!st->name().empty()) {
        return "%" + structName(*st);
      }
      std::ostringstream oss;
      oss << "{";
      for (size_t i = 0; i < st->fields().size(); ++i) {
        if (i)
          oss << ", ";
        oss << typeToString(st->fields()[i]);
      }
      oss << "}";
      return oss.str();
    }
    case TypeKind::Function: {
      auto ft = std::static_pointer_cast<const FunctionType>(ty);
      std::ostringstream oss;
      oss << typeToString(ft->returnType()) << " (";
      for (size_t i = 0; i < ft->paramTypes().size(); ++i) {
        if (i)
          oss << ", ";
        oss << typeToString(ft->paramTypes()[i]);
      }
      if (ft->isVarArg()) {
        if (!ft->paramTypes().empty())
          oss << ", ";
        oss << "...";
      }
      oss << ")";
      return oss.str();
    }
    }
    return "<unknown>";
  }

  // Collect identified struct definitions to print once per module
  void collectIdentifiedStructs(const Module &mod) {
    auto collectType = [&](const TypePtr &t, auto &&self) -> void {
      switch (t->kind()) {
      case TypeKind::Struct: {
        auto st = std::static_pointer_cast<const StructType>(t);
        if (!st->name().empty()) {
          auto id = structName(*st);
          if (identifiedNames_.count(id)) {
            break;
          }
          identifiedNames_.insert(id);
          // Build body text
          std::ostringstream b;
          b << "{";
          for (size_t i = 0; i < st->fields().size(); ++i) {
            if (i)
              b << ", ";
            b << typeToString(st->fields()[i]);
          }
          b << "}";
          identifiedStructBody_[id] = b.str();
          structDeclOrder_.push_back(id);
        }
        // Recurse into fields
        for (const auto &f : st->fields())
          self(f, self);
        break;
      }
      case TypeKind::Pointer: {
        auto pt = std::static_pointer_cast<const PointerType>(t);
        self(pt->pointee(), self);
        break;
      }
      case TypeKind::Array: {
        auto at = std::static_pointer_cast<const ArrayType>(t);
        self(at->elem(), self);
        break;
      }
      case TypeKind::Function: {
        auto ft = std::static_pointer_cast<const FunctionType>(t);
        self(ft->returnType(), self);
        for (const auto &p : ft->paramTypes())
          self(p, self);
        break;
      }
      default:
        break;
      }
    };

    for (const auto &fn : mod.functions()) {
      collectType(fn->type(), collectType);
      for (const auto &bb : fn->blocks()) {
        for (const auto &inst : bb->instructions()) {
          collectType(inst->type(), collectType);
          // Scan operands for pointer/aggregate types
          visitOperands(*inst, [&](const Value &op) {
            collectType(op.type(), collectType);
          });
        }
      }
    }
  }

  // Operand printing
  std::string valueRef(const Value &v) {
    if (auto ci = dynamic_cast<const ConstantInt *>(&v)) {
      auto ty = std::static_pointer_cast<const IntegerType>(ci->type());
      if (ty->bits() == 1)
        return ci->value() ? "true" : "false";
      return std::to_string(ci->value());
    }
    if (dynamic_cast<const ConstantNull *>(&v)) {
      return "";
    }
    if (dynamic_cast<const ConstantUnit *>(&v)) {
      return "";
    }
    return localName(&v);
  }

  std::string typedValueRef(const Value &v) {
    std::ostringstream oss;
    oss << typeToString(v.type()) << " " << valueRef(v);
    return oss.str();
  }

  // Iterate instruction operands in a generic way for type collection
  template <class F> void visitOperands(const Instruction &inst, F &&fn) {
    if (auto bi = dynamic_cast<const BinaryOpInst *>(&inst)) {
      fn(*bi->lhs());
      fn(*bi->rhs());
    } else if (auto br = dynamic_cast<const BranchInst *>(&inst)) {
      if (br->isConditional() && br->cond())
        fn(*br->cond());
    } else if (auto r = dynamic_cast<const ReturnInst *>(&inst)) {
      if (!r->isVoid() && r->value())
        fn(*r->value());
    } else if (auto a = dynamic_cast<const AllocaInst *>(&inst)) {
      if (a->arraySize())
        fn(*a->arraySize());
    } else if (auto ld = dynamic_cast<const LoadInst *>(&inst)) {
      fn(*ld->pointer());
    } else if (auto st = dynamic_cast<const StoreInst *>(&inst)) {
      fn(*st->value());
      fn(*st->pointer());
    } else if (auto gep = dynamic_cast<const GetElementPtrInst *>(&inst)) {
      fn(*gep->basePointer());
      for (const auto &idx : gep->indices())
        fn(*idx);
    } else if (auto ic = dynamic_cast<const ICmpInst *>(&inst)) {
      fn(*ic->lhs());
      fn(*ic->rhs());
    } else if (auto call = dynamic_cast<const CallInst *>(&inst)) {
      if (call->callee())
        fn(*call->callee());
      for (const auto &a2 : call->args())
        fn(*a2);
    } else if (auto phi = dynamic_cast<const PhiInst *>(&inst)) {
      for (const auto &inc : phi->incomings())
        if (inc.first)
          fn(*inc.first);
    } else if (auto sel = dynamic_cast<const SelectInst *>(&inst)) {
      fn(*sel->cond());
      fn(*sel->ifTrue());
      fn(*sel->ifFalse());
    }
  }

  // Block/function emission
  void emitBasicBlock(const BasicBlock &bb) {
    out_ << localLabel(&bb) << ":\n";
    for (const auto &inst : bb.instructions()) {
      out_ << "  ";
      emitInstruction(*inst);
      out_ << "\n";
    }
  }

  void emitInstruction(const Instruction &inst) {
    if (auto bi = dynamic_cast<const BinaryOpInst *>(&inst)) {
      const char *op = nullptr;
      switch (bi->op()) {
      case BinaryOpKind::ADD:
        op = "add";
        break;
      case BinaryOpKind::SUB:
        op = "sub";
        break;
      case BinaryOpKind::MUL:
        op = "mul";
        break;
      case BinaryOpKind::SDIV:
        op = "sdiv";
        break;
      case BinaryOpKind::SREM:
        op = "srem";
        break;
      case BinaryOpKind::SHL:
        op = "shl";
        break;
      case BinaryOpKind::ASHR:
        op = "ashr";
        break;
      case BinaryOpKind::AND:
        op = "and";
        break;
      case BinaryOpKind::OR:
        op = "or";
        break;
      case BinaryOpKind::XOR:
        op = "xor";
        break;
      }
      out_ << localName(bi) << " = " << op << " " << typeToString(bi->type())
           << " " << valueRef(*bi->lhs()) << ", " << valueRef(*bi->rhs());
      return;
    }
    if (auto br = dynamic_cast<const BranchInst *>(&inst)) {
      if (br->isConditional()) {
        out_ << "br i1 " << valueRef(*br->cond()) << ", label %"
             << localLabel(br->dest().get()) << ", label %"
             << localLabel(br->altDest().get());
      } else {
        out_ << "br label %" << localLabel(br->dest().get());
      }
      return;
    }
    if (auto r = dynamic_cast<const ReturnInst *>(&inst)) {
      if (r->isVoid()) {
        out_ << "ret void";
      } else {
        // Type of return is implied by function signature; but we print typed
        // value
        out_ << "ret " << typedValueRef(*r->value());
      }
      return;
    }
    if (auto a = dynamic_cast<const AllocaInst *>(&inst)) {
      out_ << localName(a) << " = alloca " << typeToString(a->allocatedType());
      if (a->arraySize()) {
        out_ << ", " << typedValueRef(*a->arraySize());
      }
      if (a->alignment()) {
        out_ << ", align " << a->alignment();
      }
      return;
    }
    if (auto ld = dynamic_cast<const LoadInst *>(&inst)) {
      out_ << localName(ld) << " = load ";
      if (ld->isVolatile())
        out_ << "volatile ";
      out_ << typeToString(ld->type()) << ", ptr "
           << valueRef(*ld->pointer());
      if (ld->alignment())
        out_ << ", align " << ld->alignment();
      return;
    }
    if (auto st = dynamic_cast<const StoreInst *>(&inst)) {
      out_ << "store ";
      if (st->isVolatile())
        out_ << "volatile ";
      out_ << typedValueRef(*st->value()) << ", ptr "
           << valueRef(*st->pointer());
      if (st->alignment())
        out_ << ", align " << st->alignment();
      return;
    }
    if (auto gep = dynamic_cast<const GetElementPtrInst *>(&inst)) {
      out_ << localName(gep) << " = getelementptr ";
      // Source element type and base pointer type
      // Note: We constructed result type as PointerType(sourceElemTy)
      auto basePtrTy = std::static_pointer_cast<const PointerType>(
          gep->basePointer()->type());
      out_ << typeToString(basePtrTy->pointee()) << ", ptr "
           << valueRef(*gep->basePointer());
      for (const auto &idx : gep->indices()) {
        out_ << ", " << typedValueRef(*idx);
      }
      return;
    }
    if (auto ic = dynamic_cast<const ICmpInst *>(&inst)) {
      out_ << localName(ic) << " = icmp " << icmpPredToString(ic->pred()) << " "
           << typeToString(ic->lhs()->type()) << " " << valueRef(*ic->lhs())
           << ", " << valueRef(*ic->rhs());
      return;
    }
    if (auto call = dynamic_cast<const CallInst *>(&inst)) {
      const bool hasResult = call->type()->kind() != TypeKind::Void;
      if (hasResult)
        out_ << localName(call) << " = ";
      out_ << "call " << typeToString(call->type()) << " "
           << calleeRef(*call->callee()) << "(";
      for (size_t i = 0; i < call->args().size(); ++i) {
        if (i)
          out_ << ", ";
        out_ << typedValueRef(*call->args()[i]);
      }
      out_ << ")";
      return;
    }
    if (auto phi = dynamic_cast<const PhiInst *>(&inst)) {
      out_ << localName(phi) << " = phi " << typeToString(phi->type()) << " ";
      for (size_t i = 0; i < phi->incomings().size(); ++i) {
        if (i)
          out_ << ", ";
        const auto &inc = phi->incomings()[i];
        out_ << "[ " << valueRef(*inc.first) << ", %"
             << localLabel(inc.second.get()) << " ]";
      }
      return;
    }
    if (auto sel = dynamic_cast<const SelectInst *>(&inst)) {
      out_ << localName(sel) << " = select i1 " << valueRef(*sel->cond())
           << ", " << typeToString(sel->type()) << " "
           << valueRef(*sel->ifTrue()) << ", " << typeToString(sel->type())
           << " " << valueRef(*sel->ifFalse());
      return;
    }
    if (dynamic_cast<const UnreachableInst *>(&inst)) {
      out_ << "unreachable";
      return;
    }
    if (auto zext = dynamic_cast<const ZExtInst *>(&inst)) {
      out_ << localName(zext) << " = zext " << typedValueRef(*zext->source())
           << " to " << typeToString(zext->type());
      return;
    }
    if (auto sext = dynamic_cast<const SExtInst *>(&inst)) {
      out_ << localName(sext) << " = sext " << typedValueRef(*sext->source())
           << " to " << typeToString(sext->type());
      return;
    }
    if (auto trunc = dynamic_cast<const TruncInst *>(&inst)) {
      out_ << localName(trunc) << " = trunc " << typedValueRef(*trunc->source())
           << " to " << typeToString(trunc->type());
      return;
    }

    out_ << "; <unknown instruction>";
  }

  std::string calleeRef(const Value &callee) {
    auto callee_typ =
        std::dynamic_pointer_cast<const FunctionType>(callee.type());
    if (!callee_typ || !callee_typ->function()) {
      throw std::invalid_argument("Callee must reference a function");
    }
    const auto *fnPtr = callee_typ->function().get();
    auto it = functionNames_.find(fnPtr);
    if (it == functionNames_.end()) {
      auto name = functionName(*fnPtr);
      it = functionNames_.emplace(fnPtr, name).first;
    }
    return "@" + it->second;
  }

  const char *icmpPredToString(ICmpPred p) const {
    switch (p) {
    case ICmpPred::EQ:
      return "eq";
    case ICmpPred::NE:
      return "ne";
    case ICmpPred::UGT:
      return "ugt";
    case ICmpPred::UGE:
      return "uge";
    case ICmpPred::ULT:
      return "ult";
    case ICmpPred::ULE:
      return "ule";
    case ICmpPred::SGT:
      return "sgt";
    case ICmpPred::SGE:
      return "sge";
    case ICmpPred::SLT:
      return "slt";
    case ICmpPred::SLE:
      return "sle";
    }
    return "eq";
  }
};

// Convenience free function
inline void emitLLVM(const Module &mod, std::ostream &out = std::cout) {
  LLVMEmitter emitter(out);
  emitter.emitModule(mod);
}

} // namespace rc::ir
