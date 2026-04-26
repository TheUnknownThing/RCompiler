#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/pattern.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/top_level.hpp"
#include "ast/types.hpp"
#include "lexer/lexer.hpp"
#include "semantic/analyzer/const_evaluator.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "utils/logger.hpp"

#include "context.hpp"
#include "error/exceptions.hpp"
#include "instructions/binary.hpp"
#include "instructions/control_flow.hpp"
#include "instructions/memory.hpp"
#include "instructions/misc.hpp"
#include "instructions/top_level.hpp"
#include "instructions/type.hpp"

namespace rc::ir {

using ValuePtr = std::shared_ptr<Value>;
using FuncPtr = std::shared_ptr<Function>;

// BasicBlock::isTerminated() implementation

class IREmitter : public BaseVisitor {

public:
  IREmitter() = default;
  ~IREmitter() = default;
  // Entry point
  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_,
           const Context &ctx);

  void visit(BaseNode &node) override;
  void visit(RootNode &node) override;
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(StructDecl &) override;
  void visit(EnumDecl &) override;
  void visit(TraitDecl &) override;
  void visit(ImplDecl &) override;
  void visit(LetStatement &node) override;
  void visit(ExpressionStatement &node) override;
  void visit(EmptyStatement &) override;
  void visit(NameExpression &node) override;
  void visit(LiteralExpression &node) override;
  void visit(PrefixExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(GroupExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(ReturnExpression &node) override;
  void visit(CallExpression &node) override;
  void visit(MethodCallExpression &node) override;
  void visit(FieldAccessExpression &node) override;
  void visit(StructExpression &node) override;
  void visit(UnderscoreExpression &node) override;
  void visit(BlockExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(ArrayExpression &node) override;
  void visit(IndexExpression &node) override;
  void visit(TupleExpression &) override;
  void visit(BreakExpression &node) override;
  void visit(ContinueExpression &node) override;
  void visit(PathExpression &node) override;
  void visit(QualifiedPathExpression &) override;
  void visit(BorrowExpression &node) override;
  void visit(DerefExpression &node) override;

  const Module &module() const { return module_; }
  Module &module() { return module_; }

private:
  ScopeNode *current_scope_node = nullptr;
  const Context *context = nullptr;

  Module module_{"rcompiler"};
  std::vector<FuncPtr> functions_;
  std::vector<std::shared_ptr<StructType>> struct_types_;
  std::shared_ptr<BasicBlock> current_block_;
  std::shared_ptr<BasicBlock> current_entry_block_;
  const CollectedItem *current_impl_target_ = nullptr;

  std::unordered_map<const BaseNode *, std::string> name_mangle_;
  std::unordered_map<const FunctionMetaData *, FuncPtr> function_table_;
  std::unordered_map<const FunctionMetaData *, ValuePtr> function_symbols_;
  std::unordered_map<std::string, ValuePtr> globals_;
  std::unordered_map<std::string, std::shared_ptr<ConstantString>>
      interned_strings_;

  std::vector<std::unordered_map<std::string, ValuePtr>>
      locals_; // local mapped to their memory location or SSA

  std::vector<ValuePtr> operand_stack_;
  // pair<break_target, continue_target>
  std::vector<
      std::pair<std::shared_ptr<BasicBlock>, std::shared_ptr<BasicBlock>>>
      loop_stack_;

  std::unordered_map<const FunctionMetaData *, TypePtr> sret_functions_;
  ValuePtr current_sret_ptr_;
  int next_string_id_{0};

  struct AggregateInitTarget {
    TypePtr ty;
    ValuePtr ptr;
  };
  std::vector<AggregateInitTarget> aggregate_init_targets_;

  ValuePtr pop_operand();
  void push_operand(ValuePtr v);
  ValuePtr load_ptr_value(ValuePtr v, const SemType &sem_ty);
  ValuePtr create_alloca(const TypePtr &ty, const std::string &name = {},
                        ValuePtr array_size = nullptr, unsigned alignment = 0);
  bool type_equals(const TypePtr &a, const TypePtr &b) const;
  ValuePtr lookup_local(const std::string &name) const;
  void bind_local(const std::string &name, ValuePtr v);
  void push_local_scope();
  void pop_local_scope();

  bool is_assignment(TokenType tt) const;
  bool is_integer(SemPrimitiveKind k) const;

  std::optional<BinaryOpKind> token_to_op(TokenType tt) const;

  // mangling helpers
  std::string get_scope_name(const ScopeNode *scope) const;
  std::string mangle_struct(const CollectedItem &item) const;
  std::string mangle_constant(
      const std::string &name, const ScopeNode *owner_scope,
      const std::optional<std::string> &member_of = std::nullopt) const;
  std::string mangle_function(
      const FunctionMetaData &meta, const ScopeNode *owner_scope,
      const std::optional<std::string> &member_of = std::nullopt) const;

  // lookup helpers
  CollectedItem *resolve_value_item(const std::string &name) const;
  const CollectedItem *resolve_struct_item(const std::string &name) const;
  FuncPtr find_function(const FunctionMetaData *meta) const;
  FuncPtr create_function(const std::string &name,
                          std::shared_ptr<FunctionType> ty, bool is_external,
                          const FunctionMetaData *meta);
  ValuePtr function_symbol(const FunctionMetaData &meta, const FuncPtr &fn);

  // utilities
  ValuePtr resolve_ptr(ValuePtr value, const SemType &expected,
                       const std::string &name);
  std::vector<SemType> build_effective_params(
      const FunctionMetaData &meta,
      const std::optional<SemType> &self_type = std::nullopt) const;
  TypePtr get_param_type(const SemType &param_sem_ty) const;
  SemType compute_self_type(const FunctionDecl &decl,
                            const CollectedItem *owner) const;

  FuncPtr emit_function(const FunctionMetaData &meta, const FunctionDecl &node,
                        const ScopeNode *scope,
                        const std::vector<SemType> &params,
                        const std::string &mangled_name);

  FuncPtr memset_fn_;
  FuncPtr memcpy_fn_;
  FuncPtr create_memset_fn();
  FuncPtr create_memcpy_fn();
  std::shared_ptr<ConstantString>
  intern_string_literal(const std::string &data_with_null);
  static char decode_char_literal(const std::string &literal);
  static std::string decode_string_literal(const std::string &literal);
  struct TypeLayoutInfo {
    std::size_t size;
    std::size_t align;
  };
  TypeLayoutInfo compute_type_layout(const TypePtr &ty) const;
  std::size_t compute_type_byte_size(const TypePtr &ty) const;
  bool is_zero_literal(const Expression *expr) const;
  bool is_aggregate_type(const TypePtr &ty) const;
  ValuePtr find_aggregate_init_target(const TypePtr &ty) const;
  void push_aggregate_init_target(const TypePtr &ty, ValuePtr ptr);
  void pop_aggregate_init_target();

  void emit_memcpy(ValuePtr dst, ValuePtr src, std::size_t byte_size);
};

// Implementation







































































std::size_t align_to(std::size_t value, std::size_t align);

} // namespace rc::ir
