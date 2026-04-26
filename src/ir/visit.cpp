#include "ir/visit.hpp"

namespace rc::ir {

bool BasicBlock::is_terminated() const {
  if (instructions_.empty())
    return false;
  const auto &last = instructions_.back();
  return last->is_terminator();
}
void IREmitter::run(const std::shared_ptr<RootNode> &root,
                           ScopeNode *root_scope_, const Context &ctx) {
  LOG_INFO("[IREmitter] Starting IR emission");
  if (!root || !root_scope_) {
    LOG_ERROR("[IREmitter] null root or root scope");
    throw SemanticException("IREmitter: null root or root scope");
  }
  current_scope_node = root_scope_;
  context = &ctx;
  locals_.clear();
  operand_stack_.clear();
  loop_stack_.clear();
  name_mangle_.clear();
  function_table_.clear();
  function_symbols_.clear();
  globals_.clear();
  functions_.clear();
  struct_types_.clear();
  sret_functions_.clear();
  current_sret_ptr_ = nullptr;
  aggregate_init_targets_.clear();
  interned_strings_.clear();
  next_string_id_ = 0;
  locals_.emplace_back();

  memset_fn_ = create_memset_fn();
  memcpy_fn_ = create_memcpy_fn();
  for (const auto &child : root->children) {
    if (child) {
      child->accept(*this);
    }
  }
  LOG_INFO("[IREmitter] Completed");
}
void IREmitter::visit(BaseNode &node) { node.accept(*this); }
void IREmitter::visit(FunctionDecl &node) {
  LOG_DEBUG("[IREmitter] Visiting function declaration: " + node.name);
  auto *item = resolve_value_item(node.name);
  if (!item || !item->has_function_meta()) {
    LOG_ERROR("[IREmitter] function metadata not found for " + node.name);
    throw IRException("function metadata not found for " + node.name);
  }

  const auto &meta = item->as_function_meta();
  auto params = build_effective_params(meta);
  auto mangled = mangle_function(meta, item->owner_scope);

  LOG_INFO("[IREmitter] Emitting function: " + node.name + " as " + mangled);
  emit_function(meta, node, item->owner_scope, params, mangled);
}
void IREmitter::visit(BinaryExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting BinaryExpression op:" + node.op.lexeme);

  if (node.op.type == TokenType::AND || node.op.type == TokenType::OR) {
    LOG_DEBUG("[IREmitter] Emitting short-circuit logical operator");
    node.left->accept(*this);
    auto lhs = pop_operand();
    lhs = load_ptr_value(lhs, context->lookup_type(node.left.get()));

    auto cur_func = functions_.back();
    auto rhs_block = cur_func->create_block("shortcircuit_rhs");
    auto merge_block = cur_func->create_block("shortcircuit_merge");
    auto entry_block = current_block_;

    if (node.op.type == TokenType::AND) {
      // if lhs is false, skip rhs and use false
      // if lhs is true, evaluate rhs
      current_block_->append<BranchInst>(lhs, rhs_block.get(), merge_block.get());

      current_block_ = rhs_block;
      node.right->accept(*this);
      auto rhs = pop_operand();
      rhs = load_ptr_value(rhs, context->lookup_type(node.right.get()));
      current_block_->append<BranchInst>(merge_block.get());
      auto rhs_exit_block = current_block_;

      current_block_ = merge_block;
      auto i1_type = std::make_shared<IntegerType>(1, true);
      auto false_val = ConstantInt::get_i1(false);
      std::vector<PhiInst::Incoming> incomings = {{false_val, entry_block.get()},
                                                  {rhs, rhs_exit_block.get()}};
      auto result =
          current_block_->append<PhiInst>(i1_type, incomings, "and_result");
      push_operand(result);
    } else {
      // if lhs is true, skip rhs and use true
      // if lhs is false, evaluate rhs
      current_block_->append<BranchInst>(lhs, merge_block.get(), rhs_block.get());

      current_block_ = rhs_block;
      node.right->accept(*this);
      auto rhs = pop_operand();
      rhs = load_ptr_value(rhs, context->lookup_type(node.right.get()));
      current_block_->append<BranchInst>(merge_block.get());
      auto rhs_exit_block = current_block_;

      current_block_ = merge_block;
      auto i1_type = std::make_shared<IntegerType>(1, true);
      auto true_val = ConstantInt::get_i1(true);
      std::vector<PhiInst::Incoming> incomings = {{true_val, entry_block.get()},
                                                  {rhs, rhs_exit_block.get()}};
      auto result =
          current_block_->append<PhiInst>(i1_type, incomings, "or_result");
      push_operand(result);
    }
    return;
  }

  if (is_assignment(node.op.type)) {
    node.left->accept(*this);
    auto lhs = pop_operand();

    auto lhs_ptr_ty = std::dynamic_pointer_cast<const PointerType>(lhs->type());
    if (!lhs_ptr_ty) {
      LOG_ERROR("[IREmitter] lhs is not addressable for assignment");
      throw IRException("lhs is not addressable");
    }

    bool forward_aggregate_assign = node.op.type == TokenType::ASSIGN &&
                                  is_aggregate_type(lhs_ptr_ty->pointee());
    if (forward_aggregate_assign) {
      push_aggregate_init_target(lhs_ptr_ty->pointee(), lhs);
    }
    node.right->accept(*this);
    if (forward_aggregate_assign) {
      pop_aggregate_init_target();
    }
    auto rhs = pop_operand();
    rhs = load_ptr_value(rhs, context->lookup_type(node.right.get()));

    if (node.op.type == TokenType::ASSIGN) {
      LOG_DEBUG("[IREmitter] Performing simple assignment");
      // For aggregate types, use memcpy since rhs is a pointer
      auto rhs_ptr_ty = std::dynamic_pointer_cast<const PointerType>(rhs->type());
      if (rhs_ptr_ty && is_aggregate_type(lhs_ptr_ty->pointee())) {
        if (lhs != rhs) {
          std::size_t byte_size = compute_type_byte_size(lhs_ptr_ty->pointee());
          emit_memcpy(lhs, rhs, byte_size);
        }
        push_operand(lhs);
      } else {
        current_block_->append<StoreInst>(rhs, lhs);
        push_operand(rhs);
      }
      return;
    }

    auto loaded = current_block_->append<LoadInst>(lhs, lhs_ptr_ty->pointee());
    auto op_kind = token_to_op(node.op.type);

    auto pointee_int =
        std::dynamic_pointer_cast<const IntegerType>(lhs_ptr_ty->pointee());
    bool is_unsigned = pointee_int && !pointee_int->is_signed();
    BinaryOpKind adjusted_op = *op_kind;
    if (is_unsigned) {
      switch (*op_kind) {
      case BinaryOpKind::SDIV:
        adjusted_op = BinaryOpKind::UDIV;
        break;
      case BinaryOpKind::SREM:
        adjusted_op = BinaryOpKind::UREM;
        break;
      case BinaryOpKind::ASHR:
        adjusted_op = BinaryOpKind::LSHR;
        break;
      default:
        break;
      }
    }
    auto combined = current_block_->append<BinaryOpInst>(
        adjusted_op, loaded, rhs, lhs_ptr_ty->pointee());
    current_block_->append<StoreInst>(combined, lhs);
    push_operand(combined);
    return;
  }

  if (node.op.type == TokenType::AS) {
    LOG_DEBUG("[IREmitter] Visiting cast (AS) expression");
    node.left->accept(*this);
    auto val = pop_operand();
    val = load_ptr_value(val, context->lookup_type(node.left.get()));
    auto target_ty = context->resolve_type(context->lookup_type(&node));
    auto src_int = std::dynamic_pointer_cast<const IntegerType>(val->type());
    auto dst_int = std::dynamic_pointer_cast<const IntegerType>(target_ty);

    if (src_int && dst_int && src_int->bits() == dst_int->bits() &&
        src_int->is_signed() == dst_int->is_signed()) {
      LOG_DEBUG("[IREmitter] No-op cast between identical integer types");
      push_operand(val); // no-op
      return;
    }

    if (dst_int && dst_int->bits() == 1 && src_int && src_int->bits() > 1) {
      LOG_DEBUG("[IREmitter] Emitting non-zero comparison cast to bool");
      auto zero = std::make_shared<ConstantInt>(
          std::make_shared<IntegerType>(src_int->bits(), src_int->is_signed()), 0);
      auto cmp = current_block_->append<ICmpInst>(ICmpPred::NE, val, zero,
                                                  node.op.lexeme);
      push_operand(cmp);
      return;
    }

    if (src_int && dst_int && src_int->bits() < dst_int->bits()) {
      auto zext = current_block_->append<ZExtInst>(val, target_ty, "cast");
      push_operand(zext);
      return;
    }

    if (src_int && dst_int && src_int->bits() > dst_int->bits()) {
      auto trunc = current_block_->append<TruncInst>(val, target_ty, "cast");
      push_operand(trunc);
      return;
    }

    push_operand(val);
    return;
  }

  node.left->accept(*this);
  auto lhs = pop_operand();
  node.right->accept(*this);
  auto rhs = pop_operand();
  lhs = load_ptr_value(lhs, context->lookup_type(node.left.get()));
  rhs = load_ptr_value(rhs, context->lookup_type(node.right.get()));

  auto op_kind = token_to_op(node.op.type);
  if (op_kind) {
    LOG_DEBUG("[IREmitter] Emitting binary arithmetic/logical op");

    auto lhs_int_ty = std::dynamic_pointer_cast<const IntegerType>(lhs->type());
    bool is_unsigned = lhs_int_ty && !lhs_int_ty->is_signed();
    BinaryOpKind adjusted_op = *op_kind;
    if (is_unsigned) {
      switch (*op_kind) {
      case BinaryOpKind::SDIV:
        adjusted_op = BinaryOpKind::UDIV;
        break;
      case BinaryOpKind::SREM:
        adjusted_op = BinaryOpKind::UREM;
        break;
      case BinaryOpKind::ASHR:
        adjusted_op = BinaryOpKind::LSHR;
        break;
      default:
        break;
      }
    }
    auto result =
        current_block_->append<BinaryOpInst>(adjusted_op, lhs, rhs, lhs->type());
    push_operand(result);
    return;
  }

  auto lhs_int = std::dynamic_pointer_cast<const IntegerType>(lhs->type());

  ICmpPred pred;
  switch (node.op.type) {
  case TokenType::EQ:
    pred = ICmpPred::EQ;
    break;
  case TokenType::NE:
    pred = ICmpPred::NE;
    break;
  case TokenType::LT:
    pred = lhs_int->is_signed() ? ICmpPred::SLT : ICmpPred::ULT;
    break;
  case TokenType::GT:
    pred = lhs_int->is_signed() ? ICmpPred::SGT : ICmpPred::UGT;
    break;
  case TokenType::LE:
    pred = lhs_int->is_signed() ? ICmpPred::SLE : ICmpPred::ULE;
    break;
  case TokenType::GE:
    pred = lhs_int->is_signed() ? ICmpPred::SGE : ICmpPred::UGE;
    break;
  default:
    throw IRException("unsupported binary operator: " +
                      std::to_string(static_cast<int>(node.op.type)));
  }
  auto cmp = current_block_->append<ICmpInst>(pred, lhs, rhs);
  push_operand(cmp);
}
void IREmitter::visit(ReturnExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting return expression");
  if (node.value) {
    auto ret_sem_ty = context->lookup_type(node.value->get());
    auto ret_ir_ty = context->resolve_type(ret_sem_ty);

    if (current_sret_ptr_ && is_aggregate_type(ret_ir_ty)) {
      push_aggregate_init_target(ret_ir_ty, current_sret_ptr_);
      (*node.value)->accept(*this);
      pop_aggregate_init_target();
      auto v = pop_operand();
      v = load_ptr_value(v, ret_sem_ty);
      auto val_ptr_ty = std::dynamic_pointer_cast<const PointerType>(v->type());
      if (val_ptr_ty && v != current_sret_ptr_) {
        std::size_t byte_size = compute_type_byte_size(ret_ir_ty);
        emit_memcpy(current_sret_ptr_, v, byte_size);
      }
      current_block_->append<ReturnInst>();
      return;
    }

    (*node.value)->accept(*this);
    auto v = pop_operand();

    if (current_sret_ptr_) {
      // copy result to sret pointer and return void
      v = load_ptr_value(v, ret_sem_ty);
      auto val_ptr_ty = std::dynamic_pointer_cast<const PointerType>(v->type());
      // Result is a pointer to aggregate, use memcpy
      std::size_t byte_size = compute_type_byte_size(ret_ir_ty);
      emit_memcpy(current_sret_ptr_, v, byte_size);
      current_block_->append<ReturnInst>();
      return;
    }

    if (is_aggregate_type(ret_ir_ty)) {
      throw IRException("aggregate return reached non-sret path");
    }
    v = load_ptr_value(v, ret_sem_ty);

    if (v->type()->is_void()) {
      current_block_->append<ReturnInst>();
      return;
    } else {
      current_block_->append<ReturnInst>(v);
    }
  } else {
    current_block_->append<ReturnInst>();
  }
}
void IREmitter::visit(PrefixExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting prefix operator " + node.op.lexeme);
  // visit, modify, store
  node.right->accept(*this);
  auto operand = pop_operand();
  const auto sem_ty = context->lookup_type(&node);
  operand = load_ptr_value(operand, sem_ty);
  auto ir_ty = context->resolve_type(sem_ty);

  switch (node.op.type) {
  case TokenType::NOT: {
    auto int_ty = std::dynamic_pointer_cast<const IntegerType>(ir_ty);
    if (!int_ty) {
      throw IRException("NOT operator requires integer type operand");
    }
    std::uint64_t all_ones =
        (int_ty->bits() == 64) ? ~0ULL : ((1ULL << int_ty->bits()) - 1);
    auto lhs = std::make_shared<ConstantInt>(
        std::make_shared<IntegerType>(int_ty->bits(), int_ty->is_signed()),
        all_ones);
    push_operand(current_block_->append<BinaryOpInst>(BinaryOpKind::XOR, lhs,
                                                     operand, ir_ty));
    break;
  }
  case TokenType::MINUS: {
    auto int_ty = std::dynamic_pointer_cast<const IntegerType>(ir_ty);
    auto zero_ty =
        std::make_shared<IntegerType>(int_ty->bits(), int_ty->is_signed());
    auto zero = std::make_shared<ConstantInt>(zero_ty, 0);
    push_operand(current_block_->append<BinaryOpInst>(BinaryOpKind::SUB, zero,
                                                     operand, zero_ty));
    break;
  }
  default:
    throw IRException("unsupported prefix operator");
  }
}
void IREmitter::visit(LetStatement &node) {
  LOG_DEBUG("[IREmitter] Visiting let statement");
  // alloca & store
  auto *ident = dynamic_cast<IdentifierPattern *>(node.pattern.get());

  auto sem_ty = ScopeNode::resolve_type(node.type, current_scope_node);
  auto ir_ty = context->resolve_type(sem_ty);

  auto slot = create_alloca(ir_ty, ident->name);

  if (is_aggregate_type(ir_ty)) {
    push_aggregate_init_target(ir_ty, slot);
  }
  node.expr->accept(*this);
  if (is_aggregate_type(ir_ty)) {
    pop_aggregate_init_target();
  }
  auto init = pop_operand();

  auto init_ptr_ty = std::dynamic_pointer_cast<const PointerType>(init->type());
  if (init_ptr_ty && is_aggregate_type(ir_ty)) {
    init = load_ptr_value(init, sem_ty);
    if (slot != init) {
      std::size_t byte_size = compute_type_byte_size(ir_ty);
      emit_memcpy(slot, init, byte_size);
    }
  } else {
    init = load_ptr_value(init, sem_ty);
    current_block_->append<StoreInst>(init, slot);
  }

  bind_local(ident->name, slot);
  LOG_DEBUG("[IREmitter] Bound local '" + ident->name + "'");
}
void IREmitter::visit(BlockExpression &node) {
  LOG_DEBUG("[IREmitter] Entering block expression");
  auto *previous_scope = current_scope_node;
  if (current_scope_node) {
    if (auto *child = current_scope_node->find_child_scope_by_owner(&node)) {
      current_scope_node = child;
    }
  }

  push_local_scope();

  for (const auto &stmt : node.statements) {
    if (stmt)
      stmt->accept(*this);
  }
  if (node.final_expr) {
    (*node.final_expr)->accept(*this);
  } else {
    push_operand(std::make_shared<ConstantUnit>());
  }

  pop_local_scope();
  current_scope_node = previous_scope;
  LOG_DEBUG("[IREmitter] Exiting block expression");
}
void IREmitter::visit(LiteralExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting literal: " + node.value);
  const auto sem_ty = context->lookup_type(&node);
  const auto ir_ty = context->resolve_type(sem_ty);

  if (sem_ty.is_reference()) {
    const auto &ref = sem_ty.as_reference();
    if (ref.target->is_primitive() &&
        ref.target->as_primitive().kind == SemPrimitiveKind::STR) {
      auto decoded = decode_string_literal(node.value);
      decoded.push_back('\0');
      auto cst = intern_string_literal(decoded);
      push_operand(cst);
      return;
    }
  }

  if (sem_ty.is_primitive()) {
    switch (sem_ty.as_primitive().kind) {
    case SemPrimitiveKind::BOOL: {
      bool value = node.value == "true";
      push_operand(ConstantInt::get_i1(value));
      return;
    }
    case SemPrimitiveKind::CHAR: {
      char ch = decode_char_literal(node.value);
      push_operand(std::make_shared<ConstantInt>(IntegerType::i8(false),
                                                static_cast<std::uint8_t>(ch)));
      return;
    }
    case SemPrimitiveKind::STRING:
    case SemPrimitiveKind::STR: {
      auto decoded = decode_string_literal(node.value);
      decoded.push_back('\0');
      auto cst = intern_string_literal(decoded);
      push_operand(cst);
      return;
    }
    case SemPrimitiveKind::UNIT: {
      push_operand(std::make_shared<ConstantUnit>());
      return;
    }
    default:
      break;
    }
  }

  // Integer
  auto int_ty = std::dynamic_pointer_cast<const IntegerType>(ir_ty);
  if (!int_ty) {
    throw IRException("unsupported literal type");
  }
  std::uint64_t parsed = 0;
  parsed = static_cast<std::uint64_t>(std::stoll(node.value, nullptr, 0));
  push_operand(std::make_shared<ConstantInt>(
      std::make_shared<IntegerType>(int_ty->bits(), int_ty->is_signed()), parsed));
}
void IREmitter::visit(NameExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting name expression: " + node.name);
  auto value = lookup_local(node.name);
  if (!value) {
    auto g = globals_.find(node.name);
    if (g != globals_.end()) {
      value = g->second;
      LOG_DEBUG("[IREmitter] Resolved '" + node.name + "' to global");
    }
  } else {
    LOG_DEBUG("[IREmitter] Resolved '" + node.name + "' to local");
  }

  push_operand(value);
}
void IREmitter::visit(ExpressionStatement &node) {
  LOG_DEBUG("[IREmitter] Visiting expression statement");
  if (node.expression)
    node.expression->accept(*this);
  if (node.has_semicolon && !operand_stack_.empty()) {
    LOG_DEBUG("[IREmitter] Discarding expression result due to semicolon");
    operand_stack_.pop_back(); // discard result
  }
}
void IREmitter::visit(RootNode &node) {
  LOG_DEBUG("[IREmitter] Visiting root node");
  for (const auto &child : node.children) {
    if (child)
      child->accept(*this);
  }
}
void IREmitter::visit(ConstantItem &node) {
  LOG_DEBUG("[IREmitter] Emitting constant: " + node.name);
  auto *item = resolve_value_item(node.name);

  const auto &meta = item->as_constant_meta();

  const auto &cv = *meta.evaluated_value;
  std::shared_ptr<Constant> ir_const;

  auto emit_scalar_const = [&](const ConstValue &v) -> std::shared_ptr<Constant> {
    if (v.is_bool()) {
      return ConstantInt::get_i1(v.as_bool());
    }
    if (v.is_i32()) {
      return ConstantInt::get_i32(static_cast<std::uint32_t>(v.as_i32()), true);
    }
    if (v.is_u32()) {
      return ConstantInt::get_i32(v.as_u32(), false);
    }
    if (v.is_isize()) {
      return std::make_shared<ConstantInt>(
          IntegerType::isize(), static_cast<std::uint64_t>(v.as_isize()));
    }
    if (v.is_usize()) {
      return std::make_shared<ConstantInt>(IntegerType::usize(), v.as_usize());
    }
    if (v.is_any_int()) {
      // TODO: Treat ANY_INT as i32
      return ConstantInt::get_i32(static_cast<std::uint32_t>(v.as_any_int()),
                                 true);
    }
    if (v.is_char()) {
      return std::make_shared<ConstantInt>(
          IntegerType::i8(false), static_cast<std::uint8_t>(v.as_char()));
    }
    if (v.is_string()) {
      auto decoded = decode_string_literal(v.as_string());
      decoded.push_back('\0');
      return intern_string_literal(decoded);
    }
    if (v.type.is_primitive() &&
        v.type.as_primitive().kind == SemPrimitiveKind::UNIT) {
      return std::make_shared<ConstantUnit>();
    }
    return nullptr;
  };

  std::function<std::shared_ptr<Constant>(const ConstValue &)> emit_const;
  emit_const = [&](const ConstValue &v) -> std::shared_ptr<Constant> {
    if (v.is_array()) {
      const auto &arr_sem = v.type.as_array();
      auto elem_ir_ty = context->resolve_type(*arr_sem.element);
      std::vector<std::shared_ptr<Constant>> elems;
      elems.reserve(v.as_array().size());
      for (const auto &elem : v.as_array()) {
        auto c = emit_const(elem);
        if (!c) {
          return nullptr;
        }
        elems.push_back(std::move(c));
      }
      return std::make_shared<ConstantArray>(elem_ir_ty, std::move(elems));
    }

    return emit_scalar_const(v);
  };

  ir_const = emit_const(cv);
  if (!ir_const) {
    throw IRException("constant '" + node.name +
                      "' has unsupported type for IR emission");
  }

  auto mangled = mangle_constant(meta.name, item->owner_scope);
  ir_const->set_name(mangled);
  module_.create_constant(ir_const);
  globals_[meta.name] = ir_const;
  LOG_DEBUG("[IREmitter] Registered global constant: " + meta.name +
            " mangled=" + mangled);
}
void IREmitter::visit(CallExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting call expression");
  std::vector<ValuePtr> args;
  args.reserve(node.arguments.size());
  for (const auto &arg : node.arguments) {
    if (arg) {
      arg->accept(*this);
      args.push_back(pop_operand());
    }
  }

  const FunctionMetaData *meta = nullptr;
  const ScopeNode *owner_scope = current_scope_node;
  std::optional<std::string> member_of;

  if (auto *name_expr =
          dynamic_cast<NameExpression *>(node.function_name.get())) {
    auto *item = resolve_value_item(name_expr->name);
    if (!item || !item->has_function_meta()) {
      LOG_ERROR("[IREmitter] unknown function '" + name_expr->name + "'");
      throw IRException("unknown function '" + name_expr->name + "'");
    }
    meta = &item->as_function_meta();
    owner_scope = item->owner_scope;
  } else if (auto *path_expr =
                 dynamic_cast<PathExpression *>(node.function_name.get())) {
    const std::string &type_name = path_expr->segments[0].ident;
    const std::string &fn_name = path_expr->segments[1].ident;
    const auto *type_item = resolve_struct_item(type_name);
    if (!type_item || !type_item->has_struct_meta()) {
      LOG_ERROR("[IREmitter] unknown type '" + type_name + "'");
      throw IRException("unknown type '" + type_name + "'");
    }
    const auto &meta_pack = type_item->as_struct_meta();
    for (const auto &m : meta_pack.methods) {
      if (m.name == fn_name) {
        meta = &m;
        break;
      }
    }
    owner_scope = type_item->owner_scope;
    member_of = type_item->name;
  }

  auto param_sems = build_effective_params(*meta); // handle &self

  std::vector<TypePtr> ir_params;
  ir_params.reserve(param_sems.size());
  for (const auto &p : param_sems) {
    ir_params.push_back(get_param_type(p));
  }
  auto original_ret_ty = context->resolve_type(meta->return_type);
  auto ret_ty = original_ret_ty;

  bool need_sret = is_aggregate_type(original_ret_ty);
  if (need_sret) {
    auto sret_ptr_ty = std::make_shared<PointerType>(original_ret_ty);
    ir_params.insert(ir_params.begin(), sret_ptr_ty);
    ret_ty = std::make_shared<VoidType>();
  }

  auto found = find_function(meta);
  if (!found) { // this happens for builtins or forward declarations
    LOG_DEBUG("[IREmitter] Predeclaring function '" + meta->name + "'");
    auto fn_ty = std::make_shared<FunctionType>(ret_ty, ir_params, false);
    auto mangled = mangle_function(*meta, owner_scope, member_of);
    found = create_function(mangled, fn_ty,
                            !meta->decl || !meta->decl->body.has_value(), meta);
    if (need_sret) {
      sret_functions_[meta] = original_ret_ty;
    }
  }

  auto callee = function_symbol(*meta, found);

  std::vector<ValuePtr> resolved;

  // For sret functions, create alloca
  ValuePtr sret_slot;
  if (need_sret) {
    sret_slot = find_aggregate_init_target(original_ret_ty);
    if (!sret_slot) {
      sret_slot = create_alloca(original_ret_ty, "sret_result");
    }
    resolved.push_back(sret_slot);
  }

  resolved.reserve(args.size() + (need_sret ? 1 : 0));
  for (size_t i = 0; i < args.size(); ++i) {
    resolved.push_back(
        resolve_ptr(args[i], param_sems[i], "arg" + std::to_string(i)));
  }

  LOG_DEBUG("[IREmitter] Emitting call to function " + meta->name);
  auto call_inst = current_block_->append<CallInst>(callee, resolved, ret_ty);

  if (need_sret) {
    // For sret, the result is already in sretSlot
    push_operand(sret_slot);
  } else {
    push_operand(call_inst);
  }
}
void IREmitter::visit(StructDecl &node) {
  LOG_DEBUG("[IREmitter] Visiting struct declaration: " + node.name);
  auto *item = current_scope_node
                   ? current_scope_node->find_type_item(node.name)
                   : nullptr;
  const auto &meta = item->as_struct_meta();
  std::vector<std::pair<std::string, TypePtr>> fields;
  for (const auto &[field_name, field_type] : meta.named_fields) {
    auto ir_ty = context->resolve_type(field_type);
    fields.push_back(std::make_pair(field_name, ir_ty));
  }

  auto mangled = mangle_struct(*item);
  name_mangle_[&node] = mangled;
  auto struct_type = module_.create_struct_type(fields, mangled);
  struct_types_.push_back(struct_type);
  LOG_INFO("[IREmitter] Created struct type: " + node.name +
           " mangled=" + mangled);

  // Emit evaluated associated constants
  for (const auto &c : meta.constants) {
    if (!c.evaluated_value) {
      continue;
    }
    const auto &cv = *c.evaluated_value;
    std::shared_ptr<Constant> ir_const;
    if (cv.is_bool()) {
      ir_const = ConstantInt::get_i1(cv.as_bool());
    } else if (cv.is_i32()) {
      ir_const =
          ConstantInt::get_i32(static_cast<std::uint32_t>(cv.as_i32()), true);
    } else if (cv.is_u32()) {
      ir_const = ConstantInt::get_i32(cv.as_u32(), false);
    } else if (cv.is_isize()) {
      ir_const = std::make_shared<ConstantInt>(
          IntegerType::isize(), static_cast<std::uint64_t>(cv.as_isize()));
    } else if (cv.is_usize()) {
      ir_const =
          std::make_shared<ConstantInt>(IntegerType::usize(), cv.as_usize());
    } else if (cv.is_any_int()) {
      ir_const = ConstantInt::get_i32(static_cast<std::uint32_t>(cv.as_any_int()),
                                    true);
    } else if (cv.is_char()) {
      ir_const = std::make_shared<ConstantInt>(
          IntegerType::i8(false), static_cast<std::uint8_t>(cv.as_char()));
    } else if (cv.is_string()) {
      auto decoded = decode_string_literal(cv.as_string());
      decoded.push_back('\0');
      ir_const = std::make_shared<ConstantString>(decoded);
    } else if (cv.type.is_primitive() &&
               cv.type.as_primitive().kind == SemPrimitiveKind::UNIT) {
      ir_const = std::make_shared<ConstantUnit>();
    } else {
      continue;
    }
    auto mangled_const = mangle_constant(c.name, item->owner_scope, item->name);
    ir_const->set_name(mangled_const);
    module_.create_constant(ir_const);
    globals_[c.name] = ir_const;
    LOG_DEBUG("[IREmitter] Emitted associated const: " + c.name +
              " mangled=" + mangled_const);
  }

  // create functions, but do not emit bodies yet
  for (const auto &m : meta.methods) {
    std::optional<SemType> self_type;
    if (m.decl && m.decl->self_param) {
      self_type = compute_self_type(*m.decl, item);
    }
    auto params = build_effective_params(m, self_type);
    std::vector<TypePtr> ir_params;
    ir_params.reserve(params.size());
    for (const auto &p : params) {
      ir_params.push_back(get_param_type(p));
    }
    auto original_ret_ty = context->resolve_type(m.return_type);
    auto ret_ty = original_ret_ty;

    // Check if this method should use sret
    bool need_sret = is_aggregate_type(original_ret_ty);
    if (need_sret) {
      auto sret_ptr_ty = std::make_shared<PointerType>(original_ret_ty);
      ir_params.insert(ir_params.begin(), sret_ptr_ty);
      ret_ty = std::make_shared<VoidType>();
      sret_functions_[&m] = original_ret_ty;
    }

    auto fn_ty = std::make_shared<FunctionType>(ret_ty, ir_params, false);
    auto mangled_fn = mangle_function(m, item->owner_scope, item->name);
    create_function(mangled_fn, fn_ty, !m.decl || !m.decl->body.has_value(), &m);
    LOG_DEBUG("[IREmitter] Precreated method function: " + m.name +
              " mangled=" + mangled_fn);
  }

  return;
}
void IREmitter::visit(EnumDecl &) {
  throw std::runtime_error("EnumDecl emission not implemented");
}
void IREmitter::visit(TraitDecl &) {
  throw std::runtime_error("TraitDecl emission not implemented");
}
void IREmitter::visit(ImplDecl &node) {
  LOG_DEBUG("[IREmitter] Visiting impl declaration");

  const auto &segments = node.target_type.as_path().segments;
  const std::string &target_name = segments[0];

  const auto *struct_item = resolve_struct_item(target_name);

  const auto &meta = struct_item->as_struct_meta();
  std::unordered_set<const FunctionDecl *> visited;
  auto *prev_impl = current_impl_target_;
  current_impl_target_ = struct_item;

  for (const auto &assoc : node.associated_items) {
    if (!assoc)
      continue;

    if (auto *fn = dynamic_cast<FunctionDecl *>(assoc.get())) {
      const FunctionMetaData *found = nullptr;
      for (const auto &m : meta.methods) {
        if (m.decl == fn) {
          found = &m;
          break;
        }
      }
      if (!visited.insert(fn).second) {
        continue;
      }

      std::optional<SemType> self_type;
      if (fn->self_param) {
        self_type = compute_self_type(*fn, struct_item);
      }
      auto params = build_effective_params(*found, self_type);
      auto mangled =
          mangle_function(*found, struct_item->owner_scope, struct_item->name);
      LOG_INFO("[IREmitter] Emitting impl method: " + found->name +
               " for type " + struct_item->name);
      emit_function(*found, *fn, struct_item->owner_scope, params, mangled);
    }
  }
  current_impl_target_ = prev_impl;
}
void IREmitter::visit(EmptyStatement &) {}
void IREmitter::visit(GroupExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting grouped expression");
  if (node.inner)
    node.inner->accept(*this);
}
void IREmitter::visit(IfExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting if expression");
  node.condition->accept(*this);
  auto cond_val = pop_operand();
  cond_val = load_ptr_value(cond_val, context->lookup_type(node.condition.get()));

  auto cur_func = functions_.back();
  auto then_block = cur_func->create_block("if_then");
  auto result_sem_ty = context->lookup_type(&node);
  auto result_ir_ty = context->resolve_type(result_sem_ty);
  auto entry_block = current_block_;

  bool use_result_slot = is_aggregate_type(result_ir_ty);
  ValuePtr result_slot;
  if (use_result_slot) {
    result_slot = create_alloca(result_ir_ty, "if_result");
  }

  if (node.else_block) {
    auto else_block = cur_func->create_block("if_else");
    auto merge_block = cur_func->create_block("if_merge");
    current_block_->append<BranchInst>(cond_val, then_block.get(),
                                       else_block.get());
    // then block
    current_block_ = then_block;
    if (use_result_slot) {
      push_aggregate_init_target(result_ir_ty, result_slot);
    }
    if (node.then_block)
      node.then_block->accept(*this);
    if (use_result_slot) {
      pop_aggregate_init_target();
    }
    auto then_val = pop_operand();
    auto then_terminated = current_block_->is_terminated();
    auto then_exit_block = current_block_;
    if (!then_terminated) {
      if (use_result_slot) {
        if (then_val != result_slot) {
          std::size_t byte_size = compute_type_byte_size(result_ir_ty);
          emit_memcpy(result_slot, then_val, byte_size);
        }
      } else {
        then_val = load_ptr_value(then_val, result_sem_ty);
      }
      current_block_->append<BranchInst>(merge_block.get());
    }
    // else block
    current_block_ = else_block;
    if (use_result_slot) {
      push_aggregate_init_target(result_ir_ty, result_slot);
    }
    if (node.else_block)
      std::static_pointer_cast<BlockExpression>(node.else_block.value())
          ->accept(*this);
    if (use_result_slot) {
      pop_aggregate_init_target();
    }
    auto else_val = pop_operand();
    auto else_terminated = current_block_->is_terminated();
    auto else_exit_block = current_block_;
    if (!else_terminated) {
      if (use_result_slot) {
        if (else_val != result_slot) {
          std::size_t byte_size = compute_type_byte_size(result_ir_ty);
          emit_memcpy(result_slot, else_val, byte_size);
        }
      } else {
        else_val = load_ptr_value(else_val, result_sem_ty);
      }
      current_block_->append<BranchInst>(merge_block.get());
    }
    // merge block
    current_block_ = merge_block;
    if (then_terminated && else_terminated) {
      current_block_->append<UnreachableInst>();
      push_operand(std::make_shared<ConstantUnit>());
      return;
    }
    if (result_ir_ty->is_void()) {
      push_operand(std::make_shared<ConstantUnit>());
      return;
    }

    if (use_result_slot) {
      push_operand(result_slot);
    } else {
      std::vector<PhiInst::Incoming> incomings;
      if (!then_terminated) {
        incomings.push_back({then_val, then_exit_block.get()});
      }
      if (!else_terminated) {
        incomings.push_back({else_val, else_exit_block.get()});
      }
      if (incomings.empty()) {
        current_block_->append<UnreachableInst>();
        push_operand(std::make_shared<ConstantUnit>());
        return;
      }
      auto phi =
          current_block_->append<PhiInst>(result_ir_ty, incomings, "ifval");
      push_operand(phi);
    }
  } else {
    auto merge_block = cur_func->create_block("if_merge");
    current_block_->append<BranchInst>(cond_val, then_block.get(),
                                       merge_block.get());
    // then block
    current_block_ = then_block;
    if (use_result_slot) {
      push_aggregate_init_target(result_ir_ty, result_slot);
    }
    if (node.then_block)
      node.then_block->accept(*this);
    if (use_result_slot) {
      pop_aggregate_init_target();
    }
    auto then_val = pop_operand();
    auto then_terminated = current_block_->is_terminated();
    auto then_exit_block = current_block_;
    if (!then_terminated) {
      if (use_result_slot) {
        if (then_val != result_slot) {
          std::size_t byte_size = compute_type_byte_size(result_ir_ty);
          emit_memcpy(result_slot, then_val, byte_size);
        }
      } else {
        then_val = load_ptr_value(then_val, result_sem_ty);
      }
      current_block_->append<BranchInst>(merge_block.get());
    }
    // merge block
    current_block_ = merge_block;
    auto unit = std::make_shared<ConstantUnit>();
    if (result_ir_ty->is_void()) {
      push_operand(unit);
      return;
    }

    if (use_result_slot) {
      push_operand(result_slot);
    } else {
      std::vector<PhiInst::Incoming> incomings;
      if (!then_terminated) {
        incomings.push_back({then_val, then_exit_block.get()});
      }
      incomings.push_back({unit, entry_block.get()});
      auto phi =
          current_block_->append<PhiInst>(result_ir_ty, incomings, "ifval");
      push_operand(phi);
    }
  }
}
void IREmitter::visit(MethodCallExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting method call: " + node.method_name.name);

  node.receiver->accept(*this);
  auto receiver_val = pop_operand();
  auto recv_sem = context->lookup_type(node.receiver.get());
  SemType lookup_type = recv_sem;
  if (lookup_type.is_reference()) {
    lookup_type = *lookup_type.as_reference().target;
  }

  const CollectedItem *ci = lookup_type.as_named().item;

  const auto &meta = ci->as_struct_meta();
  const FunctionMetaData *found = nullptr;
  for (const auto &m : meta.methods) {
    if (m.name == node.method_name.name) {
      found = &m;
      break;
    }
  }

  std::optional<SemType> self_sem;
  if (found->decl && found->decl->self_param) {
    self_sem = compute_self_type(*found->decl, ci);
  }

  auto param_sems = build_effective_params(*found, self_sem);

  std::vector<TypePtr> param_ir;
  param_ir.reserve(param_sems.size());
  for (const auto &p : param_sems) {
    param_ir.push_back(get_param_type(p));
  }
  auto original_ret_ty = context->resolve_type(found->return_type);
  auto ret_ty = original_ret_ty;

  bool need_sret = is_aggregate_type(original_ret_ty);
  if (need_sret) {
    auto sret_ptr_ty = std::make_shared<PointerType>(original_ret_ty);
    param_ir.insert(param_ir.begin(), sret_ptr_ty);
    ret_ty = std::make_shared<VoidType>();
  }

  auto found_fn = find_function(found);
  if (!found_fn) {
    auto fn_ty = std::make_shared<FunctionType>(ret_ty, param_ir, false);
    auto mangled = mangle_function(*found, ci->owner_scope, ci->name);
    found_fn = create_function(
        mangled, fn_ty, !found->decl || !found->decl->body.has_value(), found);
    if (need_sret) {
      sret_functions_[found] = original_ret_ty;
    }
    LOG_DEBUG("[IREmitter] Predeclared method function: " + found->name +
              " mangled=" + mangled);
  }
  auto callee = function_symbol(*found, found_fn);

  std::vector<ValuePtr> args;
  args.reserve(param_sems.size() + (need_sret ? 1 : 0));

  ValuePtr sret_slot;
  if (need_sret) {
    sret_slot = find_aggregate_init_target(original_ret_ty);
    if (!sret_slot) {
      sret_slot = create_alloca(original_ret_ty, "sret_result");
    }
    args.push_back(sret_slot);
  }

  size_t arg_index = 0;

  if (self_sem) {
    args.push_back(resolve_ptr(receiver_val, *self_sem, "self"));
    arg_index = 1;
  }

  for (size_t i = 0; i < node.arguments.size(); ++i) {
    if (node.arguments[i]) {
      node.arguments[i]->accept(*this);
      auto v = pop_operand();
      args.push_back(resolve_ptr(v, param_sems[arg_index + i],
                                 "arg" + std::to_string(arg_index + i)));
    }
  }

  auto call = current_block_->append<CallInst>(callee, args, ret_ty);

  if (need_sret) {
    push_operand(sret_slot);
  } else {
    push_operand(call);
  }
  LOG_DEBUG("[IREmitter] Emitted method call to " + found->name);
}
void IREmitter::visit(FieldAccessExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting field access: " + node.field_name);
  node.target->accept(*this);
  auto target_val = pop_operand();

  auto target_type = context->lookup_type(node.target.get());
  if (target_type.is_reference()) {
    target_type = *target_type.as_reference().target;
  }

  const CollectedItem *ci = target_type.as_named().item;
  const auto &meta = ci->as_struct_meta();
  size_t index = 0;
  SemType field_sem;
  for (size_t i = 0; i < meta.named_fields.size(); ++i) {
    if (meta.named_fields[i].first == node.field_name) {
      index = i;
      field_sem = meta.named_fields[i].second;
      break;
    }
  }

  auto struct_ir_ty = context->resolve_type(SemType::named(ci));
  auto struct_ptr =
      std::dynamic_pointer_cast<const PointerType>(target_val->type())
          ? target_val
          : nullptr;
  if (!struct_ptr) {
    LOG_ERROR("[IREmitter] field access target is not addressable");
    throw IRException("field access target is not addressable");
  }

  auto struct_ptr_ty =
      std::dynamic_pointer_cast<const PointerType>(struct_ptr->type());
  while (struct_ptr_ty && !type_equals(struct_ptr_ty->pointee(), struct_ir_ty)) {
    struct_ptr =
        current_block_->append<LoadInst>(struct_ptr, struct_ptr_ty->pointee());
    struct_ptr_ty =
        std::dynamic_pointer_cast<const PointerType>(struct_ptr->type());
  }

  auto zero = ConstantInt::get_i32(0, false);
  auto idx_const = ConstantInt::get_i32(static_cast<std::uint32_t>(index), false);
  auto field_ir_ty = context->resolve_type(field_sem);
  auto gep = current_block_->append<GetElementPtrInst>(
      field_ir_ty, struct_ptr, std::vector<ValuePtr>{zero, idx_const},
      node.field_name);
  push_operand(gep);
  LOG_DEBUG("[IREmitter] Loaded field '" + node.field_name + "'");
}
void IREmitter::visit(StructExpression &node) {
  LOG_DEBUG("[IREmitter] Emitting struct expression");

  auto *name_expr = dynamic_cast<NameExpression *>(node.path_expr.get());

  const auto *item = resolve_struct_item(name_expr->name);
  const auto &meta = item->as_struct_meta();

  std::unordered_map<std::string, std::shared_ptr<Expression>> provided;
  for (const auto &f : node.fields) {
    provided[f.name] = f.value.value();
  }

  auto struct_sem = SemType::named(item);
  auto struct_ir_ty = context->resolve_type(struct_sem);
  auto slot = find_aggregate_init_target(struct_ir_ty);
  if (!slot) {
    slot = create_alloca(struct_ir_ty, "structtmp");
  }

  auto zero = ConstantInt::get_i32(0, false);
  for (size_t i = 0; i < meta.named_fields.size(); ++i) {
    const auto &field = meta.named_fields[i];
    auto it = provided.find(field.first);
    if (it == provided.end()) {
      throw IRException("missing initializer for field '" + field.first + "'");
    }
    auto idx_const = ConstantInt::get_i32(static_cast<std::uint32_t>(i), false);
    auto field_ty = context->resolve_type(field.second);
    auto gep = current_block_->append<GetElementPtrInst>(
        field_ty, slot, std::vector<ValuePtr>{zero, idx_const}, field.first);

    if (is_aggregate_type(field_ty)) {
      push_aggregate_init_target(field_ty, gep);
      it->second->accept(*this);
      pop_aggregate_init_target();
      auto val = pop_operand();
      val = load_ptr_value(val, field.second);
      auto val_ptr_ty = std::dynamic_pointer_cast<const PointerType>(val->type());
      if (val_ptr_ty && val != gep) {
        std::size_t byte_size = compute_type_byte_size(field_ty);
        emit_memcpy(gep, val, byte_size);
      } else if (!val_ptr_ty) {
        auto resolved = resolve_ptr(val, field.second, field.first);
        current_block_->append<StoreInst>(resolved, gep);
      }
    } else {
      it->second->accept(*this);
      auto val = pop_operand();
      auto resolved = resolve_ptr(val, field.second, field.first);
      current_block_->append<StoreInst>(resolved, gep);
    }
  }

  push_operand(slot);
  LOG_DEBUG("[IREmitter] Constructed struct instance for " + name_expr->name);
}
void IREmitter::visit(UnderscoreExpression &) {
  LOG_DEBUG("[IREmitter] Visiting underscore expression (unit)");
  push_operand(std::make_shared<ConstantUnit>());
}
void IREmitter::visit(LoopExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting loop expression");
  auto cur_func = functions_.back();
  auto body_block = cur_func->create_block("loop_body");
  auto after_block = cur_func->create_block("loop_after");

  current_block_->append<BranchInst>(body_block.get());

  current_block_ = body_block;
  // break -> afterBlock, continue -> bodyBlock
  loop_stack_.push_back({after_block, body_block});
  node.body->accept(*this);
  if (!current_block_->is_terminated()) {
    current_block_->append<BranchInst>(body_block.get());
  }
  loop_stack_.pop_back();

  current_block_ = after_block;
  push_operand(std::make_shared<ConstantUnit>());
  LOG_DEBUG("[IREmitter] Finished loop expression");
}
void IREmitter::visit(WhileExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting while expression");
  auto cur_func = functions_.back();
  auto cond_block = cur_func->create_block("while_cond");
  auto body_block = cur_func->create_block("while_body");
  auto after_block = cur_func->create_block("while_after");

  current_block_->append<BranchInst>(cond_block.get());

  current_block_ = cond_block;
  node.condition->accept(*this);
  auto cond_val = pop_operand();
  cond_val = load_ptr_value(cond_val, context->lookup_type(node.condition.get()));
  current_block_->append<BranchInst>(cond_val, body_block.get(),
                                     after_block.get());

  current_block_ = body_block;
  // break -> afterBlock, continue -> condBlock
  loop_stack_.push_back({after_block, cond_block});
  node.body->accept(*this);
  if (!current_block_->is_terminated()) {
    current_block_->append<BranchInst>(cond_block.get());
  }
  loop_stack_.pop_back();

  current_block_ = after_block;
  push_operand(std::make_shared<ConstantUnit>());
  LOG_DEBUG("[IREmitter] Finished while expression");
}
void IREmitter::visit(ArrayExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting array expression");

  auto sem_ty = context->lookup_type(&node);
  const auto &arr_sem = sem_ty.as_array();
  std::size_t count = static_cast<std::size_t>(arr_sem.size);
  if (node.actual_size >= 0) {
    count = static_cast<std::size_t>(node.actual_size);
  }

  auto arr_ir_ty = context->resolve_type(sem_ty);
  auto elem_ir_ty = context->resolve_type(*arr_sem.element);
  auto slot = find_aggregate_init_target(arr_ir_ty);
  if (!slot) {
    slot = create_alloca(arr_ir_ty, "arrtmp");
  }
  auto zero = ConstantInt::get_i32(0, false);

  if (node.repeat) {
    if (is_zero_literal(node.repeat->first.get())) {
      std::size_t byte_size = compute_type_byte_size(arr_ir_ty);
      LOG_DEBUG("[IREmitter] Using memset optimization for zero-initialized "
                "array of " +
                std::to_string(byte_size) + " bytes");
      auto memset_fn = memset_fn_;
      auto ptr_ty = std::make_shared<PointerType>(IntegerType::i32(false));
      // memset(dest, value=0, size)
      std::vector<ValuePtr> args;
      args.push_back(slot);
      args.push_back(ConstantInt::get_i32(0, true));
      args.push_back(
          ConstantInt::get_i32(static_cast<std::uint32_t>(byte_size), false));

      auto memset_symbol =
          std::make_shared<Value>(memset_fn->type(), memset_fn->name());
      current_block_->append<CallInst>(memset_symbol, args, ptr_ty);
    } else {
      bool elem_is_aggregate = is_aggregate_type(elem_ir_ty);
      std::size_t repeat_count = count;
      if (elem_is_aggregate) {
        std::size_t elem_byte_size = compute_type_byte_size(elem_ir_ty);
        ValuePtr first_elt_ptr;

        if (repeat_count > 0) {
          auto first_idx = ConstantInt::get_i32(0, false);
          first_elt_ptr = current_block_->append<GetElementPtrInst>(
              elem_ir_ty, slot, std::vector<ValuePtr>{zero, first_idx}, "elt");
          push_aggregate_init_target(elem_ir_ty, first_elt_ptr);
          node.repeat->first->accept(*this);
          pop_aggregate_init_target();
          auto first_val = pop_operand();
          first_val = load_ptr_value(first_val, *arr_sem.element);
          auto first_val_ptr_ty =
              std::dynamic_pointer_cast<const PointerType>(first_val->type());
          if (first_val_ptr_ty && first_val != first_elt_ptr) {
            emit_memcpy(first_elt_ptr, first_val, elem_byte_size);
          } else if (!first_val_ptr_ty) {
            auto resolved = resolve_ptr(first_val, *arr_sem.element, "arr_init");
            current_block_->append<StoreInst>(resolved, first_elt_ptr);
          }
        } else {
          node.repeat->first->accept(*this);
          (void)pop_operand();
        }

        if (repeat_count > 1) {
          // Clone from element 0 into [1..N)
          auto cur_func = functions_.back();
          auto preheader = current_block_;
          auto cond_block = cur_func->create_block("arr_repeat_memcpy_cond");
          auto body_block = cur_func->create_block("arr_repeat_memcpy_body");
          auto after_block = cur_func->create_block("arr_repeat_memcpy_after");

          preheader->append<BranchInst>(cond_block.get());

          current_block_ = cond_block;
          auto i0 = ConstantInt::get_i32(1, false);
          auto n = ConstantInt::get_i32(static_cast<std::uint32_t>(repeat_count),
                                       false);
          std::vector<PhiInst::Incoming> incomings = {{i0, preheader.get()}};
          auto i = current_block_->append<PhiInst>(IntegerType::i32(false),
                                                   incomings, "i");
          auto cmp = current_block_->append<ICmpInst>(ICmpPred::ULT, i, n,
                                                      "arr_repeat_cmp");
          current_block_->append<BranchInst>(cmp, body_block.get(),
                                             after_block.get());

          current_block_ = body_block;
          auto gep = current_block_->append<GetElementPtrInst>(
              elem_ir_ty, slot, std::vector<ValuePtr>{zero, i}, "elt");
          emit_memcpy(gep, first_elt_ptr, elem_byte_size);
          auto one = ConstantInt::get_i32(1, false);
          auto i_next = current_block_->append<BinaryOpInst>(
              BinaryOpKind::ADD, i, one, IntegerType::i32(false), "i_next");
          current_block_->append<BranchInst>(cond_block.get());
          i->add_incoming(i_next, body_block.get());

          current_block_ = after_block;
        }

      } else {
        node.repeat->first->accept(*this);
        auto val = pop_operand();
        auto cur_func = functions_.back();
        auto resolved = resolve_ptr(val, *arr_sem.element, "arr_init");
        auto preheader = current_block_;
        auto cond_block = cur_func->create_block("arr_repeat_store_cond");
        auto body_block = cur_func->create_block("arr_repeat_store_body");
        auto after_block = cur_func->create_block("arr_repeat_store_after");

        preheader->append<BranchInst>(cond_block.get());
        current_block_ = cond_block;
        auto i0 = ConstantInt::get_i32(0, false);
        auto n =
            ConstantInt::get_i32(static_cast<std::uint32_t>(repeat_count), false);
        std::vector<PhiInst::Incoming> incomings = {{i0, preheader.get()}};
        auto i = current_block_->append<PhiInst>(IntegerType::i32(false),
                                                 incomings, "i");
        auto cmp = current_block_->append<ICmpInst>(ICmpPred::ULT, i, n,
                                                    "arr_repeat_cmp");
        current_block_->append<BranchInst>(cmp, body_block.get(),
                                           after_block.get());
        current_block_ = body_block;
        auto gep = current_block_->append<GetElementPtrInst>(
            elem_ir_ty, slot, std::vector<ValuePtr>{zero, i}, "elt");
        current_block_->append<StoreInst>(resolved, gep);
        auto one = ConstantInt::get_i32(1, false);
        auto i_next = current_block_->append<BinaryOpInst>(
            BinaryOpKind::ADD, i, one, IntegerType::i32(false), "i_next");
        current_block_->append<BranchInst>(cond_block.get());
        i->add_incoming(i_next, body_block.get());

        current_block_ = after_block;
      }
    }
  } else {
    bool elem_is_aggregate = is_aggregate_type(elem_ir_ty);
    std::size_t elem_byte_size =
        elem_is_aggregate ? compute_type_byte_size(elem_ir_ty) : 0;

    for (std::size_t i = 0; i < node.elements.size(); ++i) {
      auto idx_const = ConstantInt::get_i32(static_cast<std::uint32_t>(i), false);
      auto gep = current_block_->append<GetElementPtrInst>(
          elem_ir_ty, slot, std::vector<ValuePtr>{zero, idx_const}, "elt");

      if (elem_is_aggregate) {
        push_aggregate_init_target(elem_ir_ty, gep);
        node.elements[i]->accept(*this);
        pop_aggregate_init_target();
        auto val = pop_operand();
        val = load_ptr_value(val, *arr_sem.element);
        auto val_ptr_ty =
            std::dynamic_pointer_cast<const PointerType>(val->type());
        if (val_ptr_ty && val != gep) {
          emit_memcpy(gep, val, elem_byte_size);
        } else if (!val_ptr_ty) {
          auto resolved =
              resolve_ptr(val, *arr_sem.element, "elt" + std::to_string(i));
          current_block_->append<StoreInst>(resolved, gep);
        }
      } else {
        node.elements[i]->accept(*this);
        auto val = pop_operand();
        auto resolved =
            resolve_ptr(val, *arr_sem.element, "elt" + std::to_string(i));
        current_block_->append<StoreInst>(resolved, gep);
      }
    }
  }

  push_operand(slot);
  LOG_DEBUG("[IREmitter] Constructed array of count " + std::to_string(count));
}
void IREmitter::visit(IndexExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting index expression");

  node.target->accept(*this);
  auto target_val = pop_operand();
  auto target_sem = context->lookup_type(node.target.get());
  if (target_sem.is_reference()) {
    target_sem = *target_sem.as_reference().target;
  }

  const auto &arr_sem = target_sem.as_array();
  auto elem_ir_ty = context->resolve_type(*arr_sem.element);
  ValuePtr base_ptr;
  if (auto ptr_ty =
          std::dynamic_pointer_cast<const PointerType>(target_val->type())) {
    auto pointee = ptr_ty->pointee();
    while (ptr_ty && !std::dynamic_pointer_cast<const ArrayType>(pointee)) {
      target_val = current_block_->append<LoadInst>(target_val, pointee);
      ptr_ty = std::dynamic_pointer_cast<const PointerType>(target_val->type());
      if (!ptr_ty) {
        break;
      }
      pointee = ptr_ty->pointee();
    }
    if (ptr_ty && std::dynamic_pointer_cast<const ArrayType>(ptr_ty->pointee())) {
      base_ptr = target_val;
    }
  } else {
    throw IRException("array target is not addressable");
  }

  node.index->accept(*this);
  auto idx_val = pop_operand();
  auto idx_ty = std::dynamic_pointer_cast<const IntegerType>(idx_val->type());
  if (!idx_ty) {
    auto loaded_idx =
        load_ptr_value(idx_val, context->lookup_type(node.index.get()));
    idx_val = loaded_idx;
    if (!std::dynamic_pointer_cast<const IntegerType>(idx_val->type())) {
      LOG_ERROR("[IREmitter] array index must be integer");
      throw IRException("array index must be integer");
    }
  }

  auto zero = ConstantInt::get_i32(0, false);
  auto gep = current_block_->append<GetElementPtrInst>(
      elem_ir_ty, base_ptr, std::vector<ValuePtr>{zero, idx_val}, "idx");
  push_operand(gep);
  LOG_DEBUG("[IREmitter] Emitted index access");
}
void IREmitter::visit(TupleExpression &) {
  LOG_ERROR("[IREmitter] TupleExpression emission not implemented");
  throw std::runtime_error("TupleExpression emission not implemented");
}
void IREmitter::visit(BreakExpression &) {
  LOG_DEBUG("[IREmitter] Visiting break expression");
  if (loop_stack_.empty()) {
    throw IRException("break used outside of loop"); // this should never happen
  }
  // break target is the first element of the pair
  auto target_block = loop_stack_.back().first;
  push_operand(std::make_shared<ConstantUnit>());
  current_block_->append<BranchInst>(target_block.get());
  // should never have dead code, but just in case
  auto cur_func = functions_.back();
  auto unreachable_block = cur_func->create_block("unreachable_after_break");
  current_block_ = unreachable_block;
}
void IREmitter::visit(ContinueExpression &) {
  LOG_DEBUG("[IREmitter] Visiting continue expression");
  if (loop_stack_.empty()) {
    throw IRException(
        "continue used outside of loop"); // this should never happen
  }
  auto target_block = loop_stack_.back().second;
  push_operand(std::make_shared<ConstantUnit>());
  current_block_->append<BranchInst>(target_block.get());
  // should never have dead code, but just in case
  auto cur_func = functions_.back();
  auto unreachable_block = cur_func->create_block("unreachable_after_continue");
  current_block_ = unreachable_block;
}
void IREmitter::visit(PathExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting path expression");

  if (node.segments.size() == 1) {
    const auto &ident = node.segments[0].ident;
    if (auto local = lookup_local(ident)) {
      LOG_DEBUG("[IREmitter] Path resolved to local '" + ident + "'");
      push_operand(local);
      return;
    }
    auto g = globals_.find(ident);
    if (g != globals_.end()) {
      LOG_DEBUG("[IREmitter] Path resolved to global '" + ident + "'");
      push_operand(g->second);
      return;
    }
    throw IRException("identifier '" + ident + "' not found");
  }

  if (node.segments.size() != 2) {
    throw IRException("only TypeName::Value paths are supported");
  }

  const std::string &type_name = node.segments[0].ident;
  const std::string &val_name = node.segments[1].ident;

  const CollectedItem *type_item = nullptr;
  if (type_name == "Self" && current_impl_target_) {
    type_item = current_impl_target_;
  } else {
    type_item = resolve_struct_item(type_name);
  }

  const ConstantMetaData *found_const = nullptr;
  for (const auto &c : type_item->as_struct_meta().constants) {
    if (c.name == val_name) {
      found_const = &c;
      break;
    }
  }

  if (!found_const || !found_const->evaluated_value) {
    throw IRException("associated constant '" + val_name +
                      "' not found or unevaluated");
  }

  // reuse existing emitted constant
  auto mangled =
      mangle_constant(val_name, type_item->owner_scope, type_item->name);
  for (const auto &c : module_.constants()) {
    if (c && c->name() == mangled) {
      globals_[val_name] = c;
      push_operand(c);
      LOG_DEBUG("[IREmitter] Resolved path to existing constant " + mangled);
      return;
    }
  }

  const auto &cv = *found_const->evaluated_value;
  std::shared_ptr<Constant> ir_const;
  if (cv.is_bool()) {
    ir_const = ConstantInt::get_i1(cv.as_bool());
  } else if (cv.is_i32()) {
    ir_const =
        ConstantInt::get_i32(static_cast<std::uint32_t>(cv.as_i32()), true);
  } else if (cv.is_u32()) {
    ir_const = ConstantInt::get_i32(cv.as_u32(), false);
  } else if (cv.is_isize()) {
    ir_const = std::make_shared<ConstantInt>(
        IntegerType::isize(), static_cast<std::uint64_t>(cv.as_isize()));
  } else if (cv.is_usize()) {
    ir_const =
        std::make_shared<ConstantInt>(IntegerType::usize(), cv.as_usize());
  } else if (cv.is_any_int()) {
    ir_const =
        ConstantInt::get_i32(static_cast<std::uint32_t>(cv.as_any_int()), true);
  } else if (cv.is_char()) {
    ir_const = std::make_shared<ConstantInt>(
        IntegerType::i8(false), static_cast<std::uint8_t>(cv.as_char()));
  } else if (cv.is_string()) {
    auto decoded = decode_string_literal(cv.as_string());
    decoded.push_back('\0');
    ir_const = std::make_shared<ConstantString>(decoded);
  } else if (cv.type.is_primitive() &&
             cv.type.as_primitive().kind == SemPrimitiveKind::UNIT) {
    ir_const = std::make_shared<ConstantUnit>();
  } else {
    throw IRException("unsupported associated constant type for '" + val_name +
                      "'");
  }

  ir_const->set_name(mangled);
  module_.create_constant(ir_const);
  globals_[val_name] = ir_const;
  push_operand(ir_const);
  LOG_DEBUG("[IREmitter] Created path constant " + mangled);
}
void IREmitter::visit(QualifiedPathExpression &) {
  LOG_ERROR("[IREmitter] QualifiedPathExpression emission not implemented");
  throw std::runtime_error("QualifiedPathExpression emission not implemented");
}
void IREmitter::visit(BorrowExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting borrow expression");

  node.right->accept(*this);
  auto value = pop_operand();

  auto target_sem = context->lookup_type(node.right.get());
  auto target_ir_ty = context->resolve_type(target_sem);

  if (auto ptr_ty =
          std::dynamic_pointer_cast<const PointerType>(value->type())) {
    LOG_DEBUG("[IREmitter] borrow returning existing pointer");
    push_operand(value);
    return;
  } else {
    throw IRException("not available for borrow");
  }
}
void IREmitter::visit(DerefExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting deref expression");
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block for deref");
    throw IRException("no active basic block");
  }

  node.right->accept(*this);
  auto ptr_val = pop_operand();

  auto right_sem = context->lookup_type(node.right.get());
  auto result_sem = context->lookup_type(&node);
  auto result_ir_ty = context->resolve_type(result_sem);

  auto current = ptr_val;
  for (int i = 0; i < 8; ++i) { // we only deref up to 8 levels deep
    auto cur_ptr_ty =
        std::dynamic_pointer_cast<const PointerType>(current->type());
    if (type_equals(cur_ptr_ty->pointee(), result_ir_ty)) {
      break;
    }
    auto inner_ptr =
        std::dynamic_pointer_cast<const PointerType>(cur_ptr_ty->pointee());
    if (!inner_ptr) {
      LOG_ERROR("[IREmitter] deref result pointer type mismatch");
      throw IRException("deref result pointer type mismatch");
    }
    current = current_block_->append<LoadInst>(current, cur_ptr_ty->pointee());
  }

  push_operand(current);
}
ValuePtr IREmitter::pop_operand() {
  if (operand_stack_.empty()) {
    LOG_ERROR("[IREmitter] operand stack underflow");
    throw IRException("operand stack underflow");
  }
  auto v = operand_stack_.back();
  operand_stack_.pop_back();
  LOG_DEBUG("[IREmitter] popOperand -> " + (v ? v->name() : "<null>"));
  return v;
}
void IREmitter::push_operand(ValuePtr v) {
  if (!v) {
    LOG_ERROR("[IREmitter] attempt to push null operand");
    throw IRException("attempt to push null operand");
  }
  LOG_DEBUG("[IREmitter] pushOperand -> " + v->name());
  operand_stack_.push_back(std::move(v));
}
ValuePtr IREmitter::create_alloca(const TypePtr &ty,
                                        const std::string &name,
                                        ValuePtr array_size,
                                        unsigned alignment) {
  return current_entry_block_->prepend<AllocaInst>(ty, std::move(array_size),
                                                   alignment, name);
}
ValuePtr IREmitter::load_ptr_value(ValuePtr v, const SemType &sem_ty) {
  if (!v) {
    throw IRException("attempt to materialize null value");
  }
  auto ptr_ty = std::dynamic_pointer_cast<const PointerType>(v->type());
  if (!ptr_ty) {
    return v;
  }

  if (!sem_ty.is_reference()) {
    // For aggregate types, DON'T load them
    if (is_aggregate_type(ptr_ty->pointee())) {
      LOG_DEBUG("[IREmitter] loadPtrValue: keeping aggregate as pointer");
      return v;
    }
    LOG_DEBUG("[IREmitter] loadPtrValue loading from pointer");
    return current_block_->append<LoadInst>(v, ptr_ty->pointee());
  }

  auto expected_ty = context->resolve_type(sem_ty);
  if (type_equals(v->type(), expected_ty)) {
    return v;
  }

  auto current = v;
  auto current_ptr_ty = ptr_ty;
  while (current_ptr_ty && !type_equals(current->type(), expected_ty)) {
    if (is_aggregate_type(current_ptr_ty->pointee())) {
      // don't load aggregates
      LOG_DEBUG("[IREmitter] loadPtrValue: stopping at aggregate pointer");
      return current;
    }
    current =
        current_block_->append<LoadInst>(current, current_ptr_ty->pointee());
    current_ptr_ty =
        std::dynamic_pointer_cast<const PointerType>(current->type());
  }

  if (type_equals(current->type(), expected_ty)) {
    LOG_DEBUG("[IREmitter] loadPtrValue peeled pointer for reference");
    return current;
  }

  throw IRException("loadPtrValue unable to match reference type");
}
bool IREmitter::type_equals(const TypePtr &a, const TypePtr &b) const {
  if (a->kind() != b->kind())
    return false;
  switch (a->kind()) {
  case TypeKind::Void:
    return true;
  case TypeKind::Integer: {
    auto ia = std::static_pointer_cast<const IntegerType>(a);
    auto ib = std::static_pointer_cast<const IntegerType>(b);
    return ia->bits() == ib->bits() && ia->is_signed() == ib->is_signed();
  }
  case TypeKind::Pointer: {
    auto pa = std::static_pointer_cast<const PointerType>(a);
    auto pb = std::static_pointer_cast<const PointerType>(b);
    return type_equals(pa->pointee(), pb->pointee());
  }
  case TypeKind::Array: {
    auto aa = std::static_pointer_cast<const ArrayType>(a);
    auto ab = std::static_pointer_cast<const ArrayType>(b);
    return aa->count() == ab->count() && type_equals(aa->elem(), ab->elem());
  }
  case TypeKind::Struct: {
    auto sa = std::static_pointer_cast<const StructType>(a);
    auto sb = std::static_pointer_cast<const StructType>(b);
    if (!sa->name().empty() || !sb->name().empty()) {
      return sa->name() == sb->name();
    }
    if (sa->fields().size() != sb->fields().size())
      return false;
    for (size_t i = 0; i < sa->fields().size(); ++i) {
      if (!type_equals(sa->fields()[i], sb->fields()[i]))
        return false;
    }
    return true;
  }
  case TypeKind::Function: {
    auto fa = std::static_pointer_cast<const FunctionType>(a);
    auto fb = std::static_pointer_cast<const FunctionType>(b);
    if (fa->is_var_arg() != fb->is_var_arg())
      return false;
    if (!type_equals(fa->return_type(), fb->return_type()))
      return false;
    if (fa->param_types().size() != fb->param_types().size())
      return false;
    for (size_t i = 0; i < fa->param_types().size(); ++i) {
      if (!type_equals(fa->param_types()[i], fb->param_types()[i]))
        return false;
    }
    return true;
  }
  }
  return false;
}
ValuePtr IREmitter::lookup_local(const std::string &name) const {
  for (auto it = locals_.rbegin(); it != locals_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return nullptr;
}
void IREmitter::bind_local(const std::string &name, ValuePtr v) {
  if (locals_.empty()) {
    locals_.emplace_back();
  }
  locals_.back()[name] = std::move(v);
  LOG_DEBUG("[IREmitter] bindLocal: " + name);
}
void IREmitter::push_local_scope() {
  locals_.emplace_back();
  LOG_DEBUG("[IREmitter] pushLocalScope depth=" +
            std::to_string(locals_.size()));
}
void IREmitter::pop_local_scope() {
  if (!locals_.empty())
    locals_.pop_back();
  LOG_DEBUG("[IREmitter] popLocalScope depth=" +
            std::to_string(locals_.size()));
}
bool IREmitter::is_assignment(TokenType tt) const {
  switch (tt) {
  case TokenType::ASSIGN:
  case TokenType::PLUS_EQ:
  case TokenType::MINUS_EQ:
  case TokenType::STAR_EQ:
  case TokenType::SLASH_EQ:
  case TokenType::PERCENT_EQ:
  case TokenType::AMPERSAND_EQ:
  case TokenType::PIPE_EQ:
  case TokenType::CARET_EQ:
  case TokenType::SHL_EQ:
  case TokenType::SHR_EQ:
    return true;
  default:
    return false;
  }
}
bool IREmitter::is_integer(SemPrimitiveKind k) const {
  return k == SemPrimitiveKind::ANY_INT || k == SemPrimitiveKind::I32 ||
         k == SemPrimitiveKind::U32 || k == SemPrimitiveKind::ISIZE ||
         k == SemPrimitiveKind::USIZE;
}
std::optional<BinaryOpKind> IREmitter::token_to_op(TokenType tt) const {
  switch (tt) {
  case TokenType::PLUS:
  case TokenType::PLUS_EQ:
    return BinaryOpKind::ADD;
  case TokenType::MINUS:
  case TokenType::MINUS_EQ:
    return BinaryOpKind::SUB;
  case TokenType::STAR:
  case TokenType::STAR_EQ:
    return BinaryOpKind::MUL;
  case TokenType::SLASH:
  case TokenType::SLASH_EQ:
    return BinaryOpKind::SDIV;
  case TokenType::PERCENT:
  case TokenType::PERCENT_EQ:
    return BinaryOpKind::SREM;
  case TokenType::SHL:
  case TokenType::SHL_EQ:
    return BinaryOpKind::SHL;
  case TokenType::SHR:
  case TokenType::SHR_EQ:
    return BinaryOpKind::ASHR;
  case TokenType::AMPERSAND:
  case TokenType::AMPERSAND_EQ:
  case TokenType::AND:
    return BinaryOpKind::AND;
  case TokenType::PIPE:
  case TokenType::PIPE_EQ:
  case TokenType::OR:
    return BinaryOpKind::OR;
  case TokenType::CARET:
  case TokenType::CARET_EQ:
    return BinaryOpKind::XOR;
  default:
    return std::nullopt;
  }
}
std::string IREmitter::get_scope_name(const ScopeNode *scope) const {
  std::vector<std::string> parts;
  for (auto *s = scope; s; s = s->parent) {
    if (!s->name.empty())
      parts.push_back(s->name);
  }
  std::reverse(parts.begin(), parts.end());
  std::string qualified;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i)
      qualified += "_";
    qualified += parts[i];
  }
  return qualified;
}
std::string IREmitter::mangle_struct(const CollectedItem &item) const {
  auto qualified = get_scope_name(item.owner_scope);
  if (!qualified.empty())
    qualified += "_";
  qualified += item.name;
  return "_Struct_" + qualified;
}
std::string
IREmitter::mangle_constant(const std::string &name,
                           const ScopeNode *owner_scope,
                           const std::optional<std::string> &member_of) const {
  std::string qualified = get_scope_name(owner_scope);
  if (member_of) {
    if (!qualified.empty())
      qualified += "_";
    qualified += *member_of;
  }
  if (!qualified.empty())
    qualified += "_";
  qualified += name;
  return "_Const_" + qualified;
}
std::string
IREmitter::mangle_function(const FunctionMetaData &meta,
                           const ScopeNode *owner_scope,
                           const std::optional<std::string> &member_of) const {
  std::string qualified = get_scope_name(owner_scope);
  if (member_of) {
    if (!qualified.empty())
      qualified += "_";
    qualified += *member_of;
  }
  if (!qualified.empty())
    qualified += "_";
  qualified += meta.name;

  if (meta.name == "main" && !member_of) {
    return "main";
  }

  return "_Function_" + qualified;
}
CollectedItem *
IREmitter::resolve_value_item(const std::string &name) const {
  for (auto *s = current_scope_node; s; s = s->parent) {
    if (auto *item = s->find_value_item(name)) {
      return item;
    }
  }
  return nullptr;
}
const CollectedItem *
IREmitter::resolve_struct_item(const std::string &name) const {
  for (auto *s = current_scope_node; s; s = s->parent) {
    if (auto *item = s->find_type_item(name)) {
      return item;
    }
  }
  return nullptr;
}
FuncPtr IREmitter::find_function(const FunctionMetaData *meta) const {
  auto it = function_table_.find(meta);
  if (it != function_table_.end()) {
    return it->second;
  }
  return nullptr;
}
ValuePtr IREmitter::function_symbol(const FunctionMetaData &meta,
                                           const FuncPtr &fn) {
  if (!fn) {
    LOG_ERROR("[IREmitter] null function symbol requested");
    throw IRException("null function symbol requested");
  }
  auto it = function_symbols_.find(&meta);
  if (it != function_symbols_.end()) {
    return it->second;
  }
  auto sym = std::make_shared<Value>(fn->type(), fn->name());
  function_symbols_[&meta] = sym;
  LOG_DEBUG("[IREmitter] Created function symbol for " + fn->name());
  return sym;
}
ValuePtr IREmitter::resolve_ptr(ValuePtr value, const SemType &expected,
                                       const std::string &name) {
  auto expected_ty = context->resolve_type(expected);
  auto val_ptr = std::dynamic_pointer_cast<const PointerType>(value->type());
  auto exp_ptr = std::dynamic_pointer_cast<const PointerType>(expected_ty);

  if (!exp_ptr && is_aggregate_type(expected_ty)) {
    if (val_ptr) {
      auto aggregate_ptr = load_ptr_value(value, expected);
      auto aggregate_ptr_ty =
          std::dynamic_pointer_cast<const PointerType>(aggregate_ptr->type());
      if (aggregate_ptr_ty && type_equals(aggregate_ptr_ty->pointee(), expected_ty)) {
        auto tmp = create_alloca(expected_ty, name.empty() ? "arg_copy" : name);
        emit_memcpy(tmp, aggregate_ptr, compute_type_byte_size(expected_ty));
        return tmp;
      }
    }
    throw IRException("resolve_ptr: aggregate value must be addressable");
  }

  if (exp_ptr) {
    if (val_ptr) {
      auto current = value;
      auto current_ptr_ty = val_ptr;
      // Peel pointer
      while (current_ptr_ty && !type_equals(current->type(), expected_ty)) {
        auto inner_ptr_ty = std::dynamic_pointer_cast<const PointerType>(
            current_ptr_ty->pointee());
        if (!inner_ptr_ty) {
          break;
        }
        // Don't load aggregates when peeling pointers
        if (is_aggregate_type(current_ptr_ty->pointee())) {
          break;
        }
        current =
            current_block_->append<LoadInst>(current, current_ptr_ty->pointee());
        current_ptr_ty = inner_ptr_ty;
      }
      if (type_equals(current->type(), expected_ty)) {
        LOG_DEBUG(
            "[IREmitter] resolve_ptr: matched pointer for expected reference");
        return current;
      }
      value = current;
      val_ptr = std::dynamic_pointer_cast<const PointerType>(value->type());
    }
    auto tmp = create_alloca(exp_ptr->pointee(), name.empty() ? "tmp" : name);

    if (val_ptr && is_aggregate_type(exp_ptr->pointee())) {
      std::size_t byte_size = compute_type_byte_size(exp_ptr->pointee());
      emit_memcpy(tmp, value, byte_size);
      LOG_DEBUG("[IREmitter] resolve_ptr: used memcpy for aggregate reference");
      return tmp;
    }

    ValuePtr to_store = value;
    if (val_ptr) {
      to_store = current_block_->append<LoadInst>(value, val_ptr->pointee());
    }
    if (!type_equals(to_store->type(), exp_ptr->pointee())) {
      throw IRException("resolve_ptr: reference type mismatch");
    }
    current_block_->append<StoreInst>(to_store, tmp);
    LOG_DEBUG("[IREmitter] resolve_ptr: created temporary pointer for value");
    return tmp;
  }

  if (val_ptr) {
    LOG_DEBUG("[IREmitter] resolve_ptr: loading from pointer");
    return current_block_->append<LoadInst>(value, expected_ty);
  }
  LOG_DEBUG("[IREmitter] resolve_ptr: value already a scalar");
  return value;
}
ValuePtr IREmitter::find_aggregate_init_target(const TypePtr &ty) const {
  if (!is_aggregate_type(ty)) {
    return nullptr;
  }
  for (auto it = aggregate_init_targets_.rbegin();
       it != aggregate_init_targets_.rend(); ++it) {
    if (type_equals(it->ty, ty)) {
      return it->ptr;
    }
  }
  return nullptr;
}
void IREmitter::push_aggregate_init_target(const TypePtr &ty,
                                               ValuePtr ptr) {
  aggregate_init_targets_.push_back({ty, ptr});
}
void IREmitter::pop_aggregate_init_target() {
  if (!aggregate_init_targets_.empty()) {
    aggregate_init_targets_.pop_back();
  }
}
std::vector<SemType> IREmitter::build_effective_params(
    const FunctionMetaData &meta,
    const std::optional<SemType> &self_type) const {
  std::vector<SemType> params;
  params.reserve(meta.param_types.size() + (self_type ? 1 : 0));
  if (self_type) {
    params.push_back(*self_type);
  }
  for (const auto &p : meta.param_types) {
    params.push_back(p);
  }
  return params;
}
TypePtr IREmitter::get_param_type(const SemType &param_sem_ty) const {
  auto ir_ty = context->resolve_type(param_sem_ty);
  if (is_aggregate_type(ir_ty)) {
    return std::make_shared<PointerType>(ir_ty);
  }
  return ir_ty;
}
SemType IREmitter::compute_self_type(const FunctionDecl &decl,
                                            const CollectedItem *owner) const {
  SemType base = decl.self_param->explicit_type.has_value()
                     ? ScopeNode::resolve_type(*decl.self_param->explicit_type,
                                               current_scope_node)
                     : SemType::named(owner);
  if (decl.self_param->is_reference) {
    base = SemType::reference(base, decl.self_param->is_mutable);
  }
  return base;
}
FuncPtr IREmitter::emit_function(const FunctionMetaData &meta,
                                        const FunctionDecl &node,
                                        const ScopeNode *scope,
                                        const std::vector<SemType> &params,
                                        const std::string &mangled_name) {
  (void)scope;
  LOG_DEBUG("[IREmitter] emit_function: " + meta.name +
            " mangled=" + mangled_name);
  std::vector<TypePtr> param_typ;
  param_typ.reserve(params.size());
  for (const auto &p : params) {
    param_typ.push_back(get_param_type(p));
  }

  auto original_ret_ty = context->resolve_type(meta.return_type);
  auto ret_ty = original_ret_ty;
  bool use_sret = is_aggregate_type(original_ret_ty);

  if (use_sret) {
    LOG_DEBUG(
        "[IREmitter] Using sret for function returning large aggregate: " +
        meta.name);
    auto sret_ptr_ty = std::make_shared<PointerType>(original_ret_ty);
    param_typ.insert(param_typ.begin(), sret_ptr_ty);
    ret_ty = std::make_shared<VoidType>();
    sret_functions_[&meta] = original_ret_ty;
  }

  auto fn_ty = std::make_shared<FunctionType>(ret_ty, param_typ, false);

  auto fn = create_function(mangled_name, fn_ty, !node.body.has_value(), &meta);
  name_mangle_[&node] = mangled_name;

  if (!node.body.has_value()) {
    LOG_DEBUG("[IREmitter] emit_function: external/forward-declared function, "
              "skipping body");
    return fn;
  }

  auto saved_block = current_block_;
  auto saved_entry = current_entry_block_;
  auto saved_sret_ptr = current_sret_ptr_;
  functions_.push_back(fn);
  auto *previous_scope = current_scope_node;
  current_block_ = fn->create_block("entry");
  current_entry_block_ = current_block_;
  push_local_scope();

  std::vector<std::string> param_names;
  size_t arg_offset = 0;

  // Handle sret parameter
  if (use_sret) {
    fn->args()[0]->set_name("sret");
    current_sret_ptr_ = fn->args()[0];
    arg_offset = 1;
    LOG_DEBUG("[IREmitter] emit_function: bound sret parameter");
  } else {
    current_sret_ptr_ = nullptr;
  }

  if (node.self_param) {
    param_names.emplace_back("self");
  }
  for (const auto &p : meta.param_names) {
    if (auto *id = dynamic_cast<IdentifierPattern *>(p.get())) {
      param_names.push_back(id->name);
    }
  }

  for (size_t i = 0; i < param_names.size(); ++i) {
    std::string param_name = param_names[i];
    size_t arg_idx = i + arg_offset;
    fn->args()[arg_idx]->set_name(param_name);
    auto param_ir_ty = param_typ[arg_idx];
    auto slot = create_alloca(param_ir_ty, param_name);
    current_block_->append<StoreInst>(fn->args()[arg_idx], slot);
    bind_local(param_name, slot);
    LOG_DEBUG("[IREmitter] emit_function: bound param '" + param_name + "'");
  }

  if (node.body.has_value()) {
    if (auto *child = previous_scope ? previous_scope->find_child_scope_by_owner(
                                          node.body->get())
                                    : nullptr) {
      current_scope_node = child;
    }
    if (use_sret) {
      push_aggregate_init_target(original_ret_ty, current_sret_ptr_);
    }
    node.body.value()->accept(*this);
    if (use_sret) {
      pop_aggregate_init_target();
    }
    ValuePtr result = operand_stack_.empty() ? nullptr : pop_operand();
    bool terminated = current_block_->is_terminated();
    if (!terminated) {
      if (use_sret) {
        // For sret, copy result to sret pointer and return void
        if (result) {
          result = load_ptr_value(result, meta.return_type);
          auto res_ptr_ty =
              std::dynamic_pointer_cast<const PointerType>(result->type());
          if (res_ptr_ty && result == current_sret_ptr_) {
            LOG_DEBUG(
                "[IREmitter] sret result already in place, no copy needed");
          } else if (res_ptr_ty && is_aggregate_type(original_ret_ty) &&
                     result != current_sret_ptr_) {
            std::size_t byte_size = compute_type_byte_size(original_ret_ty);
            emit_memcpy(current_sret_ptr_, result, byte_size);
          } else {
            throw IRException("sret aggregate result is not addressable");
          }
        }
        current_block_->append<ReturnInst>();
      } else if (fn_ty->return_type()->kind() == TypeKind::Void) {
        current_block_->append<ReturnInst>();
      } else {
        if (!result) {
          result = std::make_shared<ConstantUnit>();
        }
        auto ret_ir_ty = fn_ty->return_type();
        auto res_ptr_ty =
            std::dynamic_pointer_cast<const PointerType>(result->type());

        if (is_aggregate_type(ret_ir_ty)) {
          throw IRException("aggregate return reached non-sret path");
        }
        result = load_ptr_value(result, meta.return_type);
        current_block_->append<ReturnInst>(result);
      }
    }
    current_scope_node = previous_scope;
  }

  pop_local_scope();
  current_block_ = saved_block;
  current_entry_block_ = saved_entry;
  current_sret_ptr_ = saved_sret_ptr;
  functions_.pop_back();
  LOG_INFO("[IREmitter] Finished emitting function: " + meta.name);
  return fn;
}
FuncPtr IREmitter::create_function(const std::string &name,
                                          std::shared_ptr<FunctionType> ty,
                                          bool is_external,
                                          const FunctionMetaData *meta) {
  if (meta) {
    auto it = function_table_.find(meta);
    if (it != function_table_.end()) {
      LOG_DEBUG("[IREmitter] create_function: function already exists: " +
                it->second->name());
      return it->second;
    }
  }
  LOG_DEBUG("[IREmitter] Creating function: " + name +
            " external=" + (is_external ? "true" : "false"));
  auto fn = module_.create_function(name, ty, is_external);
  function_table_[meta] = fn;
  return fn;
}
std::shared_ptr<ConstantString>
IREmitter::intern_string_literal(const std::string &data_with_null) {
  auto it = interned_strings_.find(data_with_null);
  if (it != interned_strings_.end()) {
    return it->second;
  }

  auto name = "_StrLit_" + std::to_string(next_string_id_++);
  auto cst = std::make_shared<ConstantString>(data_with_null);
  cst->set_name(name);
  module_.create_constant(cst);
  interned_strings_[data_with_null] = cst;
  return cst;
}
char IREmitter::decode_char_literal(const std::string &literal) {
  if (literal.size() < 3 || literal.front() != '\'' || literal.back() != '\'') {
    throw IRException("invalid char literal: " + literal);
  }

  auto ensure_ascii = [](unsigned value) -> char {
    if (value > 0x7F) {
      throw IRException("non-ASCII char literal value");
    }
    return static_cast<char>(value);
  };

  if (literal[1] != '\\') {
    return ensure_ascii(static_cast<unsigned char>(literal[1]));
  }

  if (literal.size() < 4) {
    throw IRException("truncated escape in char literal: " + literal);
  }

  const char esc = literal[2];
  switch (esc) {
  case '\\':
    return '\\';
  case '\'':
    return '\'';
  case '"':
    return '"';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case '0':
    return '\0';
  case 'x': {
    if (literal.size() < 6)
      throw IRException("escape too short in char literal");
    auto hex_val = [](char ch) -> int {
      if (ch >= '0' && ch <= '9')
        return ch - '0';
      if (ch >= 'a' && ch <= 'f')
        return 10 + (ch - 'a');
      if (ch >= 'A' && ch <= 'F')
        return 10 + (ch - 'A');
      return -1;
    };
    int hi = hex_val(literal[3]);
    int lo = hex_val(literal[4]);
    if (hi < 0 || lo < 0) {
      throw IRException("invalid hex escape in char literal");
    }
    return ensure_ascii(static_cast<unsigned>((hi << 4) | lo));
  }
  default:
    return ensure_ascii(static_cast<unsigned char>(esc));
  }
}
std::string IREmitter::decode_string_literal(const std::string &literal) {
  if (literal.empty()) {
    return {};
  }

  std::size_t pos = 0;
  if (literal[pos] == 'c') {
    ++pos; // c"..." treated the same as normal
  }

  bool is_raw = false;
  std::size_t hash_count = 0;
  if (pos < literal.size() && literal[pos] == 'r') {
    is_raw = true;
    ++pos;
    while (pos < literal.size() && literal[pos] == '#') {
      ++hash_count;
      ++pos;
    }
  }

  auto first_quote = literal.find('"', pos);
  auto last_quote = literal.rfind('"');
  if (first_quote == std::string::npos || last_quote == std::string::npos ||
      last_quote <= first_quote) {
    return literal;
  }

  (void)hash_count;

  if (is_raw) {
    return literal.substr(first_quote + 1, last_quote - first_quote - 1);
  }

  std::string body = literal.substr(first_quote + 1, last_quote - first_quote - 1);
  std::string out;
  out.reserve(body.size());

  auto hex_val = [](char ch) -> int {
    if (ch >= '0' && ch <= '9')
      return ch - '0';
    if (ch >= 'a' && ch <= 'f')
      return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F')
      return 10 + (ch - 'A');
    return -1;
  };

  auto ensure_ascii = [](unsigned value) -> char {
    if (value > 0x7F) {
      throw IRException("non-ASCII string literal value");
    }
    return static_cast<char>(value);
  };

  for (std::size_t i = 0; i < body.size(); ++i) {
    char ch = body[i];
    if (ch != '\\') {
      out.push_back(ch);
      continue;
    }
    if (i + 1 >= body.size()) {
      throw IRException("dangling escape in string literal");
    }
    char esc = body[++i];
    switch (esc) {
    case '\\':
      out.push_back('\\');
      break;
    case '"':
      out.push_back('"');
      break;
    case '\'':
      out.push_back('\'');
      break;
    case 'n':
      out.push_back('\n');
      break;
    case 'r':
      out.push_back('\r');
      break;
    case 't':
      out.push_back('\t');
      break;
    case '0':
      out.push_back('\0');
      break;
    case 'x': {
      if (i + 2 >= body.size()) {
        throw IRException("\\x escape too short in string literal");
      }
      int hi = hex_val(body[i + 1]);
      int lo = hex_val(body[i + 2]);
      if (hi < 0 || lo < 0) {
        throw IRException("invalid hex escape in string literal");
      }
      out.push_back(ensure_ascii(static_cast<unsigned>((hi << 4) | lo)));
      i += 2;
      break;
    }
    default:
      out.push_back(ensure_ascii(static_cast<unsigned char>(esc)));
      break;
    }
  }

  return out;
}
FuncPtr IREmitter::create_memset_fn() {
  // Signature: ptr @_Function_prelude_builtin_memset(ptr, i32, i32)
  auto ptr_ty = std::make_shared<PointerType>(IntegerType::i32(false));
  auto i32_ty = IntegerType::i32(true);
  auto fn_ty = std::make_shared<FunctionType>(
      ptr_ty, std::vector<TypePtr>{ptr_ty, i32_ty, i32_ty}, false);
  memset_fn_ =
      module_.create_function("_Function_prelude_builtin_memset", fn_ty, true);
  LOG_DEBUG("[IREmitter] Created builtin_memset function declaration");
  return memset_fn_;
}
FuncPtr IREmitter::create_memcpy_fn() {
  // Signature: ptr @_Function_prelude_builtin_memcpy(ptr, ptr, i32)
  auto ptr_ty =
      std::make_shared<PointerType>(std::make_shared<IntegerType>(32, false));
  auto i32_ty = IntegerType::i32(true);
  auto fn_ty = std::make_shared<FunctionType>(
      ptr_ty, std::vector<TypePtr>{ptr_ty, ptr_ty, i32_ty}, false);
  memcpy_fn_ =
      module_.create_function("_Function_prelude_builtin_memcpy", fn_ty, true);
  LOG_DEBUG("[IREmitter] Created builtin_memcpy function declaration");
  return memcpy_fn_;
}
std::size_t align_to(std::size_t value, std::size_t align) {
  if (align <= 1) {
    return value;
  }
  auto rem = value % align;
  return rem ? (value + (align - rem)) : value;
}
IREmitter::TypeLayoutInfo
IREmitter::compute_type_layout(const TypePtr &ty) const {
  if (auto int_ty = std::dynamic_pointer_cast<const IntegerType>(ty)) {
    std::size_t size = (int_ty->bits() + 7) / 8;
    if (size == 0) {
      size = 1;
    }
    return {size, size};
  }
  if (auto arr_ty = std::dynamic_pointer_cast<const ArrayType>(ty)) {
    auto elem = compute_type_layout(arr_ty->elem());
    std::size_t stride = align_to(elem.size, elem.align);
    return {stride * arr_ty->count(), elem.align};
  }
  if (auto struct_ty = std::dynamic_pointer_cast<const StructType>(ty)) {
    std::size_t offset = 0;
    std::size_t max_align = 1;
    for (const auto &field : struct_ty->fields()) {
      auto layout = compute_type_layout(field);
      offset = align_to(offset, layout.align);
      offset += layout.size;
      max_align = std::max(max_align, layout.align);
    }
    offset = align_to(offset, max_align);
    return {offset, max_align};
  }
  if (std::dynamic_pointer_cast<const PointerType>(ty)) {
    std::size_t ptr_bytes = module_.target().pointer_width
                               ? module_.target().pointer_width / 8
                               : sizeof(void *);
    return {ptr_bytes, ptr_bytes};
  }
  if (ty->is_void()) {
    return {0, 1};
  }
  throw IRException("unknown type");
}
std::size_t IREmitter::compute_type_byte_size(const TypePtr &ty) const {
  return compute_type_layout(ty).size;
}
bool IREmitter::is_zero_literal(const Expression *expr) const {
  if (auto *lit = dynamic_cast<const LiteralExpression *>(expr)) {
    if (lit->value == "false") {
      return true;
    }
    try {
      if (lit->value.empty())
        return false;
      long long val = std::stoll(lit->value, nullptr, 0);
      return val == 0;
    } catch (...) {
      return false;
    }
  }
  return false;
}
bool IREmitter::is_aggregate_type(const TypePtr &ty) const {
  return ty->kind() == TypeKind::Array || ty->kind() == TypeKind::Struct;
}
void IREmitter::emit_memcpy(ValuePtr dst, ValuePtr src,
                                  std::size_t byte_size) {
  if (byte_size == 0) {
    return;
  }
  auto memcpy_symbol =
      std::make_shared<Value>(memcpy_fn_->type(), memcpy_fn_->name());
  std::vector<ValuePtr> args;
  args.push_back(dst);
  args.push_back(src);
  args.push_back(
      ConstantInt::get_i32(static_cast<std::uint32_t>(byte_size), true));
  auto ptr_ty = std::make_shared<PointerType>(IntegerType::i32(false));
  current_block_->append<CallInst>(memcpy_symbol, args, ptr_ty);
}

} // namespace rc::ir
