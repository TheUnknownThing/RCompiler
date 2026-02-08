#pragma once

#include <cassert>
#include <cctype>
#include <iomanip>
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
#include "instructions/visitor.hpp"

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
      if (auto cs = dynamic_cast<const ConstantString *>(c.get())) {
        out_ << "@" << constantName(*c) << " = constant "
             << typeToString(cs->arrayType()) << " " << encodeStringData(*cs)
             << "\n";
      } else if (auto ca = dynamic_cast<const ConstantArray *>(c.get())) {
        out_ << "@" << constantName(*c) << " = constant "
             << typeToString(ca->arrayType()) << " " << encodeArrayData(*ca)
             << "\n";
      } else {
        out_ << "@" << constantName(*c) << " = constant "
             << typeToString(c->type()) << " " << valueRef(*c) << "\n";
      }
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
      out_ << "declare " << typeToString(fty->returnType()) << " @"
           << mangledName << "(";
    } else {
      out_ << "define " << typeToString(fty->returnType()) << " @"
           << mangledName << "(";
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
    std::string base =
        bb->name().empty() ? genBlock() : bb->name() + genBlock();
    blockNames_[bb] = base;
    return base;
  }

  std::string genTmp() { return "val" + std::to_string(nextValueId_++); }
  std::string genBlock() { return "bb" + std::to_string(nextBlockId_++); }
  std::string uniqueGlobal(const std::string &base,
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
    structNames_[&st] = base;
    return base;
  }

  std::string constantName(const Constant &c) {
    auto it = globalConstNames_.find(&c);
    if (it != globalConstNames_.end()) {
      return it->second;
    }
    auto base = c.name().empty() ? "cst" : c.name();
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
      return "ptr";
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
    if (auto cs = dynamic_cast<const ConstantString *>(&v)) {
      return "@" + constantName(*cs);
    }
    if (auto ca = dynamic_cast<const ConstantArray *>(&v)) {
      if (ca->name().empty()) {
        return encodeArrayData(*ca);
      }
      return "@" + constantName(*ca);
    }
    if (dynamic_cast<const ConstantNull *>(&v)) {
      return "";
    }
    if (dynamic_cast<const ConstantUnit *>(&v)) {
      return "";
    }
    if (dynamic_cast<const UndefValue *>(&v)) {
      return "undef";
    }
    return localName(&v);
  }

  std::string encodeStringData(const ConstantString &cs) const {
    std::ostringstream oss;
    oss << "c\"";
    for (unsigned char ch : cs.data()) {
      switch (ch) {
      case '\\':
        oss << "\\\\";
        break;
      case '"':
        oss << "\\\"";
        break;
      case '\n':
        oss << "\\0A";
        break;
      case '\r':
        oss << "\\0D";
        break;
      case '\t':
        oss << "\\09";
        break;
      default:
        if (std::isprint(ch) && ch != '"') {
          oss << static_cast<char>(ch);
        } else {
          oss << "\\" << std::uppercase << std::hex << std::setw(2)
              << std::setfill('0') << static_cast<int>(ch) << std::nouppercase
              << std::dec;
        }
        break;
      }
    }
    oss << "\"";
    return oss.str();
  }

  std::string encodeArrayData(const ConstantArray &ca) {
    auto at = std::static_pointer_cast<const ArrayType>(ca.arrayType());
    const auto &elemTy = at->elem();
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < ca.elements().size(); ++i) {
      if (i)
        oss << ", ";
      if (auto inner =
              dynamic_cast<const ConstantArray *>(ca.elements()[i].get())) {
        oss << typeToString(elemTy) << " " << encodeArrayData(*inner);
      } else {
        oss << typeToString(elemTy) << " " << valueRef(*ca.elements()[i]);
      }
    }
    oss << "]";
    return oss.str();
  }

  std::string typedValueRef(const Value &v) {
    std::ostringstream oss;
    oss << typeToString(v.type()) << " " << valueRef(v);
    return oss.str();
  }

  // Iterate instruction operands in a generic way for type collection
  template <class F> void visitOperands(const Instruction &inst, F &&fn) {
    struct OperandVisitor final : InstructionVisitor {
      F &fn;

      explicit OperandVisitor(F &f) : fn(f) {}

      void visit(const BinaryOpInst &bi) override {
        fn(*bi.lhs());
        fn(*bi.rhs());
      }

      void visit(const AllocaInst &a) override {
        if (a.arraySize()) {
          fn(*a.arraySize());
        }
      }
      void visit(const LoadInst &ld) override { fn(*ld.pointer()); }
      void visit(const StoreInst &st) override {
        fn(*st.value());
        fn(*st.pointer());
      }
      void visit(const GetElementPtrInst &gep) override {
        fn(*gep.basePointer());
        for (const auto &idx : gep.indices()) {
          fn(*idx);
        }
      }

      void visit(const BranchInst &br) override {
        if (br.isConditional() && br.cond()) {
          fn(*br.cond());
        }
      }
      void visit(const ReturnInst &r) override {
        if (!r.isVoid() && r.value()) {
          fn(*r.value());
        }
      }
      void visit(const UnreachableInst & /*u*/) override {}

      void visit(const ICmpInst &ic) override {
        fn(*ic.lhs());
        fn(*ic.rhs());
      }
      void visit(const CallInst &call) override {
        if (call.callee()) {
          fn(*call.callee());
        }
        for (const auto &arg : call.args()) {
          fn(*arg);
        }
      }
      void visit(const PhiInst &phi) override {
        for (const auto &inc : phi.incomings()) {
          if (inc.first) {
            fn(*inc.first);
          }
        }
      }
      void visit(const SelectInst &sel) override {
        fn(*sel.cond());
        fn(*sel.ifTrue());
        fn(*sel.ifFalse());
      }
      void visit(const ZExtInst &zext) override { fn(*zext.source()); }
      void visit(const SExtInst &sext) override { fn(*sext.source()); }
      void visit(const TruncInst &trunc) override { fn(*trunc.source()); }
    } visitor{fn};

    inst.accept(visitor);
  }

  // Block/function emission
  void emitBasicBlock(const BasicBlock &bb) {
    out_ << localLabel(&bb) << ":\n";
    for (const auto &inst : bb.instructions()) {
      out_ << "  ";
      emitInstruction(*inst);
      out_ << "\n";

      if (inst->isTerminator()) {
        break;
      }
    }
  }

  void emitInstruction(const Instruction &inst) {
    struct EmitVisitor final : InstructionVisitor {
      LLVMEmitter &self;

      explicit EmitVisitor(LLVMEmitter &s) : self(s) {}

      void visit(const BinaryOpInst &bi) override {
        const char *op = nullptr;
        switch (bi.op()) {
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
        case BinaryOpKind::UDIV:
          op = "udiv";
          break;
        case BinaryOpKind::SREM:
          op = "srem";
          break;
        case BinaryOpKind::UREM:
          op = "urem";
          break;
        case BinaryOpKind::SHL:
          op = "shl";
          break;
        case BinaryOpKind::ASHR:
          op = "ashr";
          break;
        case BinaryOpKind::LSHR:
          op = "lshr";
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
        self.out_ << self.localName(&bi) << " = " << op << " "
                  << self.typeToString(bi.type()) << " "
                  << self.valueRef(*bi.lhs()) << ", "
                  << self.valueRef(*bi.rhs());
      }

      void visit(const BranchInst &br) override {
        if (br.isConditional()) {
          self.out_ << "br i1 " << self.valueRef(*br.cond()) << ", label %"
                    << self.localLabel(br.dest()) << ", label %"
                    << self.localLabel(br.altDest());
        } else {
          self.out_ << "br label %" << self.localLabel(br.dest());
        }
      }

      void visit(const ReturnInst &r) override {
        if (r.isVoid()) {
          self.out_ << "ret void";
        } else {
          self.out_ << "ret " << self.typedValueRef(*r.value());
        }
      }

      void visit(const AllocaInst &a) override {
        self.out_ << self.localName(&a) << " = alloca "
                  << self.typeToString(a.allocatedType());
        if (a.arraySize()) {
          self.out_ << ", " << self.typedValueRef(*a.arraySize());
        }
        if (a.alignment()) {
          self.out_ << ", align " << a.alignment();
        }
      }

      void visit(const LoadInst &ld) override {
        self.out_ << self.localName(&ld) << " = load ";
        if (ld.isVolatile()) {
          self.out_ << "volatile ";
        }
        self.out_ << self.typeToString(ld.type()) << ", ptr "
                  << self.valueRef(*ld.pointer());
        if (ld.alignment()) {
          self.out_ << ", align " << ld.alignment();
        }
      }

      void visit(const StoreInst &st) override {
        self.out_ << "store ";
        if (st.isVolatile()) {
          self.out_ << "volatile ";
        }
        self.out_ << self.typedValueRef(*st.value()) << ", ptr "
                  << self.valueRef(*st.pointer());
        if (st.alignment()) {
          self.out_ << ", align " << st.alignment();
        }
      }

      void visit(const GetElementPtrInst &gep) override {
        self.out_ << self.localName(&gep) << " = getelementptr ";
        auto basePtrTy = std::static_pointer_cast<const PointerType>(
            gep.basePointer()->type());
        self.out_ << self.typeToString(basePtrTy->pointee()) << ", ptr "
                  << self.valueRef(*gep.basePointer());
        for (const auto &idx : gep.indices()) {
          self.out_ << ", " << self.typedValueRef(*idx);
        }
      }

      void visit(const ICmpInst &ic) override {
        self.out_ << self.localName(&ic) << " = icmp "
                  << self.icmpPredToString(ic.pred()) << " "
                  << self.typeToString(ic.lhs()->type()) << " "
                  << self.valueRef(*ic.lhs()) << ", "
                  << self.valueRef(*ic.rhs());
      }

      void visit(const CallInst &call) override {
        const bool hasResult = call.type()->kind() != TypeKind::Void;
        if (hasResult) {
          self.out_ << self.localName(&call) << " = ";
        }
        self.out_ << "call " << self.typeToString(call.type()) << " "
                  << self.calleeRef(*call.callee()) << "(";
        for (size_t i = 0; i < call.args().size(); ++i) {
          if (i) {
            self.out_ << ", ";
          }
          self.out_ << self.typedValueRef(*call.args()[i]);
        }
        self.out_ << ")";
      }

      void visit(const PhiInst &phi) override {
        self.out_ << self.localName(&phi) << " = phi "
                  << self.typeToString(phi.type()) << " ";
        for (size_t i = 0; i < phi.incomings().size(); ++i) {
          if (i) {
            self.out_ << ", ";
          }
          const auto &inc = phi.incomings()[i];
          self.out_ << "[ " << self.valueRef(*inc.first) << ", %"
                    << self.localLabel(inc.second) << " ]";
        }
      }

      void visit(const SelectInst &sel) override {
        self.out_ << self.localName(&sel) << " = select i1 "
                  << self.valueRef(*sel.cond()) << ", "
                  << self.typeToString(sel.type()) << " "
                  << self.valueRef(*sel.ifTrue()) << ", "
                  << self.typeToString(sel.type()) << " "
                  << self.valueRef(*sel.ifFalse());
      }

      void visit(const UnreachableInst & /*u*/) override {
        self.out_ << "unreachable";
      }

      void visit(const ZExtInst &zext) override {
        self.out_ << self.localName(&zext) << " = zext "
                  << self.typedValueRef(*zext.source()) << " to "
                  << self.typeToString(zext.type());
      }

      void visit(const SExtInst &sext) override {
        self.out_ << self.localName(&sext) << " = sext "
                  << self.typedValueRef(*sext.source()) << " to "
                  << self.typeToString(sext.type());
      }

      void visit(const TruncInst &trunc) override {
        self.out_ << self.localName(&trunc) << " = trunc "
                  << self.typedValueRef(*trunc.source()) << " to "
                  << self.typeToString(trunc.type());
      }
    } visitor{*this};

    inst.accept(visitor);
  }

  std::string calleeRef(const Value &callee) {
    auto callee_typ =
        std::dynamic_pointer_cast<const FunctionType>(callee.type());
    if (!callee_typ || !callee_typ->function()) {
      throw std::invalid_argument("Callee must reference a function");
    }
    const auto *fnPtr = callee_typ->function();
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
