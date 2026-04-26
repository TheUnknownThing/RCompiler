#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
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
  void visit(const ir::BasicBlock &basic_block);

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

  size_t reg_id_ = 1;
  size_t unique_name_id_ = 1; // for generating unique labels
  std::unordered_map<const ir::BasicBlock *, std::string> block_names_;
  std::unordered_map<const ir::Function *, std::string> function_names_;
  std::unordered_map<const ir::Value *, std::shared_ptr<AsmOperand>>
      value_operand_map_;

  const ir::Function *current_function_{nullptr};
  const ir::BasicBlock *current_ir_block_{nullptr};

  AsmBlock *current_block_{nullptr};
  size_t stack_offset_ = 0;
  const ir::BasicBlock *entry_ir_block_{nullptr};

  std::string describe(const ir::Value *value) const;
  std::shared_ptr<AsmOperand>
  resolve_operand_or_immediate(const std::shared_ptr<ir::Value> &value,
                            const char *reason,
                            const ir::Instruction *inst = nullptr);

  bool is_aggregate_type(const ir::TypePtr &ty) const;

  struct TypeLayoutInfo {
    size_t size;
    size_t align;
  };
  TypeLayoutInfo compute_type_layout(const ir::TypePtr &ty) const;
  size_t compute_type_byte_size(const ir::TypePtr &ty) const;
  size_t align_to(size_t offset, size_t align) const;
  std::shared_ptr<Register>
  materialize_address(const std::shared_ptr<AsmOperand> &ptr,
                     const ir::Instruction *inst, const char *reason);
  void emit_add_immediate(const std::shared_ptr<Register> &dst,
                        const std::shared_ptr<AsmOperand> &lhs,
                        int64_t imm_value);
  std::shared_ptr<AsmOperand>
  emit_scaled_index(const std::shared_ptr<ir::Value> &index, size_t stride,
                  const ir::Instruction *inst);

  std::string get_unique_label(const ir::BasicBlock *block) {
    if (block_names_.count(block) == 0) {
      block_names_[block] = block->parent()->name() + "." + block->name() + "." +
                           std::to_string(unique_name_id_++);
    }
    return block_names_[block];
  }

  std::string get_unique_function_name(const ir::Function *function) {
    if (function_names_.count(function) == 0) {
      function_names_[function] = function->name();
    }
    return function_names_[function];
  }

  std::shared_ptr<Register> create_virtual_register();
  std::shared_ptr<Register> get_or_create_result_register(const ir::Value *value);
  std::shared_ptr<Register> create_physical_register(int id);
  std::shared_ptr<Immediate> create_immediate(int32_t value);
  std::shared_ptr<StackSlot> create_stack_slot(const ir::TypePtr &type);
  std::shared_ptr<StackSlot> create_incoming_arg_slot(size_t arg_index);
  std::shared_ptr<Symbol> create_label_operand(const ir::BasicBlock *block);
  std::shared_ptr<Symbol> create_function_operand(const ir::Function *function);
  Immediate *as_immediate(const std::shared_ptr<AsmOperand> &op);
  std::shared_ptr<AsmOperand> get_reg(const std::shared_ptr<AsmOperand> &op);
  std::shared_ptr<AsmOperand>
  prepare_call_argument(const std::shared_ptr<ir::Value> &arg,
                      const ir::CallInst &call);
};

} // namespace rc::backend
