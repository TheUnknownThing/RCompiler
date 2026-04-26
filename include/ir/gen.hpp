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
#include "instructions/control_flow.hpp"
#include "instructions/memory.hpp"
#include "instructions/misc.hpp"
#include "instructions/top_level.hpp"
#include "instructions/visitor.hpp"

namespace rc::ir {

class LLVMEmitter {
public:
  explicit LLVMEmitter(std::ostream &out = std::cout) : out_(out) {}

  // Entry points
  void emit_module(const Module &mod) {
    reset();
    collect_identified_structs(mod);
    // Header comments
    out_ << "; ModuleID = '" << mod.name() << "'\n";
    if (!mod.target().data_layout.empty()) {
      out_ << "target datalayout = \"" << mod.target().data_layout << "\"\n";
    }
    if (!mod.target().triple.empty()) {
      out_ << "target triple = \"" << mod.target().triple << "\"\n";
    }
    // Identified struct declarations
    for (const auto &decl : struct_decl_order_) {
      out_ << "%" << decl << " = type " << identified_struct_body_.at(decl)
           << "\n";
    }
    if (!struct_decl_order_.empty())
      out_ << "\n";

    for (const auto &c : mod.constants()) {
      if (!c)
        continue;
      if (auto cs = dynamic_cast<const ConstantString *>(c.get())) {
        out_ << "@" << constant_name(*c) << " = constant "
             << type_to_string(cs->array_type()) << " " << encode_string_data(*cs)
             << "\n";
      } else if (auto ca = dynamic_cast<const ConstantArray *>(c.get())) {
        out_ << "@" << constant_name(*c) << " = constant "
             << type_to_string(ca->array_type()) << " " << encode_array_data(*ca)
             << "\n";
      } else {
        out_ << "@" << constant_name(*c) << " = constant "
             << type_to_string(c->type()) << " " << value_ref(*c) << "\n";
      }
    }
    if (!mod.constants().empty())
      out_ << "\n";

    // Functions
    for (const auto &fn : mod.functions()) {
      emit_function(*fn);
      out_ << "\n";
    }
  }

  void emit_function(const Function &fn) {
    name_state_.clear();
    block_names_.clear();
    next_value_id_ = 0;
    next_block_id_ = 0;

    const auto &fty = fn.type();
    const bool is_decl = fn.is_external();
    std::string mangled_name = function_name(fn);

    if (is_decl) {
      out_ << "declare " << type_to_string(fty->return_type()) << " @"
           << mangled_name << "(";
    } else {
      out_ << "define " << type_to_string(fty->return_type()) << " @"
           << mangled_name << "(";
    }

    // Arguments
    for (size_t i = 0; i < fn.args().size(); ++i) {
      if (i)
        out_ << ", ";
      const auto &arg = fn.args()[i];
      const auto &pty = fty->param_types()[i];
      out_ << type_to_string(pty) << " " << local_name(arg.get());
    }
    out_ << ")";

    if (is_decl) {
      out_ << "\n";
      return;
    }

    out_ << " {\n";

    // Pre-assign basic block names
    for (const auto &bb : fn.blocks()) {
      (void)local_label(bb.get());
    }

    for (const auto &bb : fn.blocks()) {
      emit_basic_block(*bb);
    }

    out_ << "}" << "\n";
  }

private:
  std::ostream &out_;

  // Per-module state for identified structs
  std::unordered_set<std::string> identified_names_;
  std::unordered_map<std::string, std::string> identified_struct_body_;
  std::vector<std::string> struct_decl_order_;

  // Per-function naming state
  std::unordered_map<const Value *, std::string> name_state_;
  std::unordered_map<const BasicBlock *, std::string> block_names_;
  std::unordered_map<const Function *, std::string> function_names_;
  std::unordered_map<const StructType *, std::string> struct_names_;
  std::unordered_map<const Constant *, std::string> global_const_names_;
  std::unordered_map<std::string, int> function_name_counters_;
  int next_value_id_{0};
  int next_block_id_{0};

  void reset() {
    identified_names_.clear();
    identified_struct_body_.clear();
    struct_decl_order_.clear();
    function_names_.clear();
    struct_names_.clear();
    global_const_names_.clear();
    function_name_counters_.clear();
  }

  // Naming helpers
  std::string local_name(const Value *v) {
    auto it = name_state_.find(v);
    if (it != name_state_.end())
      return "%" + it->second;
    std::string base = v->name().empty() ? gen_tmp() : v->name() + gen_tmp();
    name_state_[v] = base;
    return "%" + base;
  }

  std::string local_label(const BasicBlock *bb) {
    auto it = block_names_.find(bb);
    if (it != block_names_.end())
      return it->second;
    std::string base =
        bb->name().empty() ? gen_block() : bb->name() + gen_block();
    block_names_[bb] = base;
    return base;
  }

  std::string gen_tmp() { return "val" + std::to_string(next_value_id_++); }
  std::string gen_block() { return "bb" + std::to_string(next_block_id_++); }
  std::string unique_global(const std::string &base,
                           std::unordered_map<std::string, int> &counters) {
    auto &ctr = counters[base];
    std::string name = base;
    if (ctr > 0) {
      name += "." + std::to_string(ctr);
    }
    ++ctr;
    return name;
  }

  std::string function_name(const Function &fn) {
    auto it = function_names_.find(&fn);
    if (it != function_names_.end())
      return it->second;
    auto base = fn.name().empty() ? "fn" : fn.name();
    auto mangled = unique_global(base, function_name_counters_);
    function_names_[&fn] = mangled;
    return mangled;
  }

  std::string struct_name(const StructType &st) {
    auto it = struct_names_.find(&st);
    if (it != struct_names_.end()) {
      return it->second;
    }
    auto base = st.name().empty() ? "struct" : st.name();
    struct_names_[&st] = base;
    return base;
  }

  std::string constant_name(const Constant &c) {
    auto it = global_const_names_.find(&c);
    if (it != global_const_names_.end()) {
      return it->second;
    }
    auto base = c.name().empty() ? "cst" : c.name();
    global_const_names_[&c] = base;
    return base;
  }

  // Type printing
  std::string type_to_string(const TypePtr &ty) {
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
      oss << "[" << at->count() << " x " << type_to_string(at->elem()) << "]";
      return oss.str();
    }
    case TypeKind::Struct: {
      auto st = std::static_pointer_cast<const StructType>(ty);
      if (!st->name().empty()) {
        return "%" + struct_name(*st);
      }
      std::ostringstream oss;
      oss << "{";
      for (size_t i = 0; i < st->fields().size(); ++i) {
        if (i)
          oss << ", ";
        oss << type_to_string(st->fields()[i]);
      }
      oss << "}";
      return oss.str();
    }
    case TypeKind::Function: {
      auto ft = std::static_pointer_cast<const FunctionType>(ty);
      std::ostringstream oss;
      oss << type_to_string(ft->return_type()) << " (";
      for (size_t i = 0; i < ft->param_types().size(); ++i) {
        if (i)
          oss << ", ";
        oss << type_to_string(ft->param_types()[i]);
      }
      if (ft->is_var_arg()) {
        if (!ft->param_types().empty())
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
  void collect_identified_structs(const Module &mod) {
    auto collect_type = [&](const TypePtr &t, auto &&self) -> void {
      switch (t->kind()) {
      case TypeKind::Struct: {
        auto st = std::static_pointer_cast<const StructType>(t);
        if (!st->name().empty()) {
          auto id = struct_name(*st);
          if (identified_names_.count(id)) {
            break;
          }
          identified_names_.insert(id);
          // Build body text
          std::ostringstream b;
          b << "{";
          for (size_t i = 0; i < st->fields().size(); ++i) {
            if (i)
              b << ", ";
            b << type_to_string(st->fields()[i]);
          }
          b << "}";
          identified_struct_body_[id] = b.str();
          struct_decl_order_.push_back(id);
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
        self(ft->return_type(), self);
        for (const auto &p : ft->param_types())
          self(p, self);
        break;
      }
      default:
        break;
      }
    };

    for (const auto &fn : mod.functions()) {
      collect_type(fn->type(), collect_type);
      for (const auto &bb : fn->blocks()) {
        for (const auto &inst : bb->instructions()) {
          collect_type(inst->type(), collect_type);
          // Scan operands for pointer/aggregate types
          visit_operands(*inst, [&](const Value &op) {
            collect_type(op.type(), collect_type);
          });
        }
      }
    }
  }

  // Operand printing
  std::string value_ref(const Value &v) {
    if (auto ci = dynamic_cast<const ConstantInt *>(&v)) {
      auto ty = std::static_pointer_cast<const IntegerType>(ci->type());
      if (ty->bits() == 1)
        return ci->value() ? "true" : "false";
      return std::to_string(ci->value());
    }
    if (auto cs = dynamic_cast<const ConstantString *>(&v)) {
      return "@" + constant_name(*cs);
    }
    if (auto ca = dynamic_cast<const ConstantArray *>(&v)) {
      if (ca->name().empty()) {
        return encode_array_data(*ca);
      }
      return "@" + constant_name(*ca);
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
    return local_name(&v);
  }

  std::string encode_string_data(const ConstantString &cs) const {
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

  std::string encode_array_data(const ConstantArray &ca) {
    auto at = std::static_pointer_cast<const ArrayType>(ca.array_type());
    const auto &elem_ty = at->elem();
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < ca.elements().size(); ++i) {
      if (i)
        oss << ", ";
      if (auto inner =
              dynamic_cast<const ConstantArray *>(ca.elements()[i].get())) {
        oss << type_to_string(elem_ty) << " " << encode_array_data(*inner);
      } else {
        oss << type_to_string(elem_ty) << " " << value_ref(*ca.elements()[i]);
      }
    }
    oss << "]";
    return oss.str();
  }

  std::string typed_value_ref(const Value &v) {
    std::ostringstream oss;
    oss << type_to_string(v.type()) << " " << value_ref(v);
    return oss.str();
  }

  // Iterate instruction operands in a generic way for type collection
  template <class F> void visit_operands(const Instruction &inst, F &&fn) {
    struct OperandVisitor final : InstructionVisitor {
      F &fn;

      explicit OperandVisitor(F &f) : fn(f) {}

      void visit(const BinaryOpInst &bi) override {
        fn(*bi.lhs());
        fn(*bi.rhs());
      }

      void visit(const AllocaInst &a) override {
        if (a.array_size()) {
          fn(*a.array_size());
        }
      }
      void visit(const LoadInst &ld) override { fn(*ld.pointer()); }
      void visit(const StoreInst &st) override {
        fn(*st.value());
        fn(*st.pointer());
      }
      void visit(const GetElementPtrInst &gep) override {
        fn(*gep.base_pointer());
        for (const auto &idx : gep.indices()) {
          fn(*idx);
        }
      }

      void visit(const BranchInst &br) override {
        if (br.is_conditional() && br.cond()) {
          fn(*br.cond());
        }
      }
      void visit(const ReturnInst &r) override {
        if (!r.is_void() && r.value()) {
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
        fn(*sel.if_true());
        fn(*sel.if_false());
      }
      void visit(const ZExtInst &zext) override { fn(*zext.source()); }
      void visit(const SExtInst &sext) override { fn(*sext.source()); }
      void visit(const TruncInst &trunc) override { fn(*trunc.source()); }
      void visit(const MoveInst &move) override { fn(*move.source()); }
    } visitor{fn};

    inst.accept(visitor);
  }

  // Block/function emission
  void emit_basic_block(const BasicBlock &bb) {
    out_ << local_label(&bb) << ":\n";
    for (const auto &inst : bb.instructions()) {
      out_ << "  ";
      emit_instruction(*inst);
      out_ << "\n";

      if (inst->is_terminator()) {
        break;
      }
    }
  }

  void emit_instruction(const Instruction &inst) {
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
        self.out_ << self.local_name(&bi) << " = " << op << " "
                  << self.type_to_string(bi.type()) << " "
                  << self.value_ref(*bi.lhs()) << ", "
                  << self.value_ref(*bi.rhs());
      }

      void visit(const BranchInst &br) override {
        if (br.is_conditional()) {
          self.out_ << "br i1 " << self.value_ref(*br.cond()) << ", label %"
                    << self.local_label(br.dest()) << ", label %"
                    << self.local_label(br.alt_dest());
        } else {
          self.out_ << "br label %" << self.local_label(br.dest());
        }
      }

      void visit(const ReturnInst &r) override {
        if (r.is_void()) {
          self.out_ << "ret void";
        } else {
          self.out_ << "ret " << self.typed_value_ref(*r.value());
        }
      }

      void visit(const AllocaInst &a) override {
        self.out_ << self.local_name(&a) << " = alloca "
                  << self.type_to_string(a.allocated_type());
        if (a.array_size()) {
          self.out_ << ", " << self.typed_value_ref(*a.array_size());
        }
        if (a.alignment()) {
          self.out_ << ", align " << a.alignment();
        }
      }

      void visit(const LoadInst &ld) override {
        self.out_ << self.local_name(&ld) << " = load ";
        self.out_ << self.type_to_string(ld.type()) << ", ptr "
                  << self.value_ref(*ld.pointer());
        if (ld.alignment()) {
          self.out_ << ", align " << ld.alignment();
        }
      }

      void visit(const StoreInst &st) override {
        self.out_ << "store ";
        if (st.is_volatile()) {
          self.out_ << "volatile ";
        }
        self.out_ << self.typed_value_ref(*st.value()) << ", ptr "
                  << self.value_ref(*st.pointer());
        if (st.alignment()) {
          self.out_ << ", align " << st.alignment();
        }
      }

      void visit(const GetElementPtrInst &gep) override {
        self.out_ << self.local_name(&gep) << " = getelementptr ";
        auto base_ptr_ty = std::static_pointer_cast<const PointerType>(
            gep.base_pointer()->type());
        self.out_ << self.type_to_string(base_ptr_ty->pointee()) << ", ptr "
                  << self.value_ref(*gep.base_pointer());
        for (const auto &idx : gep.indices()) {
          self.out_ << ", " << self.typed_value_ref(*idx);
        }
      }

      void visit(const ICmpInst &ic) override {
        self.out_ << self.local_name(&ic) << " = icmp "
                  << self.icmp_pred_to_string(ic.pred()) << " "
                  << self.type_to_string(ic.lhs()->type()) << " "
                  << self.value_ref(*ic.lhs()) << ", "
                  << self.value_ref(*ic.rhs());
      }

      void visit(const CallInst &call) override {
        const bool has_result = call.type()->kind() != TypeKind::Void;
        if (has_result) {
          self.out_ << self.local_name(&call) << " = ";
        }
        self.out_ << "call " << self.type_to_string(call.type()) << " "
                  << self.callee_ref(*call.callee()) << "(";
        for (size_t i = 0; i < call.args().size(); ++i) {
          if (i) {
            self.out_ << ", ";
          }
          self.out_ << self.typed_value_ref(*call.args()[i]);
        }
        self.out_ << ")";
      }

      void visit(const PhiInst &phi) override {
        self.out_ << self.local_name(&phi) << " = phi "
                  << self.type_to_string(phi.type()) << " ";
        for (size_t i = 0; i < phi.incomings().size(); ++i) {
          if (i) {
            self.out_ << ", ";
          }
          const auto &inc = phi.incomings()[i];
          self.out_ << "[ " << self.value_ref(*inc.first) << ", %"
                    << self.local_label(inc.second) << " ]";
        }
      }

      void visit(const SelectInst &sel) override {
        self.out_ << self.local_name(&sel) << " = select i1 "
                  << self.value_ref(*sel.cond()) << ", "
                  << self.type_to_string(sel.type()) << " "
                  << self.value_ref(*sel.if_true()) << ", "
                  << self.type_to_string(sel.type()) << " "
                  << self.value_ref(*sel.if_false());
      }

      void visit(const UnreachableInst & /*u*/) override {
        self.out_ << "unreachable";
      }

      void visit(const ZExtInst &zext) override {
        self.out_ << self.local_name(&zext) << " = zext "
                  << self.typed_value_ref(*zext.source()) << " to "
                  << self.type_to_string(zext.type());
      }

      void visit(const SExtInst &sext) override {
        self.out_ << self.local_name(&sext) << " = sext "
                  << self.typed_value_ref(*sext.source()) << " to "
                  << self.type_to_string(sext.type());
      }

      void visit(const TruncInst &trunc) override {
        self.out_ << self.local_name(&trunc) << " = trunc "
                  << self.typed_value_ref(*trunc.source()) << " to "
                  << self.type_to_string(trunc.type());
      }

      void visit(const MoveInst &move) override {
        self.out_ << "move " << self.typed_value_ref(*move.source()) << " "
                  << self.local_name(move.destination().get());
      }
    } visitor{*this};

    inst.accept(visitor);
  }

  std::string callee_ref(const Value &callee) {
    auto callee_typ =
        std::dynamic_pointer_cast<const FunctionType>(callee.type());
    if (!callee_typ || !callee_typ->function()) {
      throw std::invalid_argument("Callee must reference a function");
    }
    const auto *fn_ptr = callee_typ->function();
    auto it = function_names_.find(fn_ptr);
    if (it == function_names_.end()) {
      auto name = function_name(*fn_ptr);
      it = function_names_.emplace(fn_ptr, name).first;
    }
    return "@" + it->second;
  }

  const char *icmp_pred_to_string(ICmpPred p) const {
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
void emit_llvm(const Module &mod, std::ostream &out = std::cout);

} // namespace rc::ir
