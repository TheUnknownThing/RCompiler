#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
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
#include "ast/nodes/topLevel.hpp"
#include "ast/types.hpp"
#include "lexer/lexer.hpp"
#include "semantic/analyzer/constEvaluator.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "utils/logger.hpp"

#include "context.hpp"
#include "error/exceptions.hpp"
#include "instructions/binary.hpp"
#include "instructions/controlFlow.hpp"
#include "instructions/memory.hpp"
#include "instructions/misc.hpp"
#include "instructions/topLevel.hpp"
#include "instructions/type.hpp"

namespace rc::ir {

using ValuePtr = std::shared_ptr<Value>;
using FuncPtr = std::shared_ptr<Function>;

// BasicBlock::isTerminated() implementation
inline bool BasicBlock::isTerminated() const {
  if (instructions_.empty())
    return false;
  const auto &last = instructions_.back();
  return dynamic_cast<BranchInst *>(last.get()) != nullptr ||
         dynamic_cast<ReturnInst *>(last.get()) != nullptr ||
         dynamic_cast<UnreachableInst *>(last.get()) != nullptr;
}

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

  ValuePtr popOperand();
  void pushOperand(ValuePtr v);
  ValuePtr loadPtrValue(ValuePtr v, const SemType &semTy);
  ValuePtr createAlloca(const TypePtr &ty, const std::string &name = {},
                        ValuePtr arraySize = nullptr, unsigned alignment = 0);
  bool typeEquals(const TypePtr &a, const TypePtr &b) const;
  ValuePtr lookupLocal(const std::string &name) const;
  void bindLocal(const std::string &name, ValuePtr v);
  void pushLocalScope();
  void popLocalScope();

  bool isAssignment(TokenType tt) const;
  bool isInteger(SemPrimitiveKind k) const;

  std::optional<BinaryOpKind> tokenToOP(TokenType tt) const;

  // mangling helpers
  std::string getScopeName(const ScopeNode *scope) const;
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
  SemType compute_self_type(const FunctionDecl &decl,
                            const CollectedItem *owner) const;

  FuncPtr emit_function(const FunctionMetaData &meta, const FunctionDecl &node,
                        const ScopeNode *scope,
                        const std::vector<SemType> &params,
                        const std::string &mangled_name);

  FuncPtr memset_fn_;
  FuncPtr memcpy_fn_;
  FuncPtr createMemsetFn();
  FuncPtr createMemcpyFn();
  std::shared_ptr<ConstantString>
  internStringLiteral(const std::string &data_with_null);
  static char decodeCharLiteral(const std::string &literal);
  static std::string decodeStringLiteral(const std::string &literal);
  struct TypeLayoutInfo {
    std::size_t size;
    std::size_t align;
  };
  TypeLayoutInfo computeTypeLayout(const TypePtr &ty) const;
  std::size_t computeTypeByteSize(const TypePtr &ty) const;
  bool isZeroLiteral(const Expression *expr) const;
  bool isAggregateType(const TypePtr &ty) const;

  void emitMemcpy(ValuePtr dst, ValuePtr src, std::size_t byteSize);
};

// Implementation

inline void IREmitter::run(const std::shared_ptr<RootNode> &root,
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
  interned_strings_.clear();
  next_string_id_ = 0;
  locals_.emplace_back();

  memset_fn_ = createMemsetFn();
  memcpy_fn_ = createMemcpyFn();
  for (const auto &child : root->children) {
    if (child) {
      child->accept(*this);
    }
  }
  LOG_INFO("[IREmitter] Completed");
}

inline void IREmitter::visit(BaseNode &node) {
  LOG_DEBUG(std::string("[IREmitter] visit(BaseNode) dispatch for ") +
            typeid(node).name());
  if (auto *decl = dynamic_cast<FunctionDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<StructDecl *>(&node)) {
    visit(*decl);
  } else if (auto *cst = dynamic_cast<ConstantItem *>(&node)) {
    visit(*cst);
  } else if (auto *decl = dynamic_cast<EnumDecl *>(&node)) {
    visit(*decl);
  } else if (auto *decl = dynamic_cast<TraitDecl *>(&node)) {
    visit(*decl);
  } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
    visit(*expr);
  } else if (auto *stmt = dynamic_cast<LetStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<ExpressionStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *stmt = dynamic_cast<EmptyStatement *>(&node)) {
    visit(*stmt);
  } else if (auto *expr = dynamic_cast<NameExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LiteralExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<PrefixExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BinaryExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<GroupExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IfExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ReturnExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<CallExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<MethodCallExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<FieldAccessExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<StructExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<UnderscoreExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BlockExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<LoopExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<WhileExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ArrayExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<IndexExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<TupleExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BreakExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<ContinueExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<PathExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<QualifiedPathExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<BorrowExpression *>(&node)) {
    visit(*expr);
  } else if (auto *expr = dynamic_cast<DerefExpression *>(&node)) {
    visit(*expr);
  }
}

inline void IREmitter::visit(FunctionDecl &node) {
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

inline void IREmitter::visit(BinaryExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting BinaryExpression op:" + node.op.lexeme);
  
  if (node.op.type == TokenType::AND || node.op.type == TokenType::OR) {
    LOG_DEBUG("[IREmitter] Emitting short-circuit logical operator");
    node.left->accept(*this);
    auto lhs = popOperand();
    lhs = loadPtrValue(lhs, context->lookupType(node.left.get()));

    auto cur_func = functions_.back();
    auto rhsBlock = cur_func->createBlock("shortcircuit_rhs");
    auto mergeBlock = cur_func->createBlock("shortcircuit_merge");
    auto entryBlock = current_block_;

    if (node.op.type == TokenType::AND) {
      // if lhs is false, skip rhs and use false
      // if lhs is true, evaluate rhs
      current_block_->append<BranchInst>(lhs, rhsBlock, mergeBlock);
      
      current_block_ = rhsBlock;
      node.right->accept(*this);
      auto rhs = popOperand();
      rhs = loadPtrValue(rhs, context->lookupType(node.right.get()));
      current_block_->append<BranchInst>(mergeBlock);
      auto rhsExitBlock = current_block_;

      current_block_ = mergeBlock;
      auto i1_type = std::make_shared<IntegerType>(1, true);
      auto false_val = ConstantInt::getI1(false);
      std::vector<PhiInst::Incoming> incomings = {
          {false_val, entryBlock},
          {rhs, rhsExitBlock}
      };
      auto result = current_block_->append<PhiInst>(i1_type, incomings, "and_result");
      pushOperand(result);
    } else {
      // if lhs is true, skip rhs and use true
      // if lhs is false, evaluate rhs
      current_block_->append<BranchInst>(lhs, mergeBlock, rhsBlock);
      
      current_block_ = rhsBlock;
      node.right->accept(*this);
      auto rhs = popOperand();
      rhs = loadPtrValue(rhs, context->lookupType(node.right.get()));
      current_block_->append<BranchInst>(mergeBlock);
      auto rhsExitBlock = current_block_;

      current_block_ = mergeBlock;
      auto i1_type = std::make_shared<IntegerType>(1, true);
      auto true_val = ConstantInt::getI1(true);
      std::vector<PhiInst::Incoming> incomings = {
          {true_val, entryBlock},
          {rhs, rhsExitBlock}
      };
      auto result = current_block_->append<PhiInst>(i1_type, incomings, "or_result");
      pushOperand(result);
    }
    return;
  }
  
  if (isAssignment(node.op.type)) {
    node.left->accept(*this);
    auto lhs = popOperand();
    node.right->accept(*this);
    auto rhs = popOperand();
    rhs = loadPtrValue(rhs, context->lookupType(node.right.get()));

    auto lhsPtrTy = std::dynamic_pointer_cast<const PointerType>(lhs->type());
    if (!lhsPtrTy) {
      LOG_ERROR("[IREmitter] lhs is not addressable for assignment");
      throw IRException("lhs is not addressable");
    }

    if (node.op.type == TokenType::ASSIGN) {
      LOG_DEBUG("[IREmitter] Performing simple assignment");
      // For aggregate types, use memcpy since rhs is a pointer
      auto rhsPtrTy = std::dynamic_pointer_cast<const PointerType>(rhs->type());
      if (rhsPtrTy && isAggregateType(lhsPtrTy->pointee())) {
        std::size_t byteSize = computeTypeByteSize(lhsPtrTy->pointee());
        emitMemcpy(lhs, rhs, byteSize);
        pushOperand(lhs);
      } else {
        current_block_->append<StoreInst>(rhs, lhs);
        pushOperand(rhs);
      }
      return;
    }

    auto loaded = current_block_->append<LoadInst>(lhs, lhsPtrTy->pointee());
    auto opKind = tokenToOP(node.op.type);

    auto pointeeInt =
        std::dynamic_pointer_cast<const IntegerType>(lhsPtrTy->pointee());
    bool is_unsigned = pointeeInt && !pointeeInt->isSigned();
    BinaryOpKind adjustedOp = *opKind;
    if (is_unsigned) {
      switch (*opKind) {
      case BinaryOpKind::SDIV:
        adjustedOp = BinaryOpKind::UDIV;
        break;
      case BinaryOpKind::SREM:
        adjustedOp = BinaryOpKind::UREM;
        break;
      case BinaryOpKind::ASHR:
        adjustedOp = BinaryOpKind::LSHR;
        break;
      default:
        break;
      }
    }
    auto combined = current_block_->append<BinaryOpInst>(
        adjustedOp, loaded, rhs, lhsPtrTy->pointee());
    current_block_->append<StoreInst>(combined, lhs);
    pushOperand(combined);
    return;
  }

  if (node.op.type == TokenType::AS) {
    LOG_DEBUG("[IREmitter] Visiting cast (AS) expression");
    node.left->accept(*this);
    auto val = popOperand();
    val = loadPtrValue(val, context->lookupType(node.left.get()));
    auto targetTy = context->resolveType(context->lookupType(&node));
    auto srcInt = std::dynamic_pointer_cast<const IntegerType>(val->type());
    auto dstInt = std::dynamic_pointer_cast<const IntegerType>(targetTy);

    if (srcInt && dstInt && srcInt->bits() == dstInt->bits() &&
        srcInt->isSigned() == dstInt->isSigned()) {
      LOG_DEBUG("[IREmitter] No-op cast between identical integer types");
      pushOperand(val); // no-op
      return;
    }

    if (dstInt && dstInt->bits() == 1 && srcInt && srcInt->bits() > 1) {
      LOG_DEBUG("[IREmitter] Emitting non-zero comparison cast to bool");
      auto zero = std::make_shared<ConstantInt>(
          std::make_shared<IntegerType>(srcInt->bits(), srcInt->isSigned()), 0);
      auto cmp = current_block_->append<ICmpInst>(ICmpPred::NE, val, zero,
                                                  node.op.lexeme);
      pushOperand(cmp);
      return;
    }

    if (srcInt && dstInt && srcInt->bits() < dstInt->bits()) {
      auto zext = current_block_->append<ZExtInst>(val, targetTy, "cast");
      pushOperand(zext);
      return;
    }

    if (srcInt && dstInt && srcInt->bits() > dstInt->bits()) {
      auto trunc = current_block_->append<TruncInst>(val, targetTy, "cast");
      pushOperand(trunc);
      return;
    }

    pushOperand(val);
    return;
  }

  node.left->accept(*this);
  auto lhs = popOperand();
  node.right->accept(*this);
  auto rhs = popOperand();
  lhs = loadPtrValue(lhs, context->lookupType(node.left.get()));
  rhs = loadPtrValue(rhs, context->lookupType(node.right.get()));

  auto opKind = tokenToOP(node.op.type);
  if (opKind) {
    LOG_DEBUG("[IREmitter] Emitting binary arithmetic/logical op");

    auto lhsIntTy = std::dynamic_pointer_cast<const IntegerType>(lhs->type());
    bool is_unsigned = lhsIntTy && !lhsIntTy->isSigned();
    BinaryOpKind adjustedOp = *opKind;
    if (is_unsigned) {
      switch (*opKind) {
      case BinaryOpKind::SDIV:
        adjustedOp = BinaryOpKind::UDIV;
        break;
      case BinaryOpKind::SREM:
        adjustedOp = BinaryOpKind::UREM;
        break;
      case BinaryOpKind::ASHR:
        adjustedOp = BinaryOpKind::LSHR;
        break;
      default:
        break;
      }
    }
    auto result =
        current_block_->append<BinaryOpInst>(adjustedOp, lhs, rhs, lhs->type());
    pushOperand(result);
    return;
  }

  auto lhsInt = std::dynamic_pointer_cast<const IntegerType>(lhs->type());

  ICmpPred pred;
  switch (node.op.type) {
  case TokenType::EQ:
    pred = ICmpPred::EQ;
    break;
  case TokenType::NE:
    pred = ICmpPred::NE;
    break;
  case TokenType::LT:
    pred = lhsInt->isSigned() ? ICmpPred::SLT : ICmpPred::ULT;
    break;
  case TokenType::GT:
    pred = lhsInt->isSigned() ? ICmpPred::SGT : ICmpPred::UGT;
    break;
  case TokenType::LE:
    pred = lhsInt->isSigned() ? ICmpPred::SLE : ICmpPred::ULE;
    break;
  case TokenType::GE:
    pred = lhsInt->isSigned() ? ICmpPred::SGE : ICmpPred::UGE;
    break;
  default:
    throw IRException("unsupported binary operator: " +
                      std::to_string(static_cast<int>(node.op.type)));
  }
  auto cmp = current_block_->append<ICmpInst>(pred, lhs, rhs);
  pushOperand(cmp);
}

inline void IREmitter::visit(ReturnExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting return expression");
  if (node.value) {
    (*node.value)->accept(*this);
    auto v = popOperand();
    auto retSemTy = context->lookupType(node.value->get());
    auto retIrTy = context->resolveType(retSemTy);

    if (current_sret_ptr_) {
      // copy result to sret pointer and return void
      auto valPtrTy = std::dynamic_pointer_cast<const PointerType>(v->type());
      // Result is a pointer to aggregate, use memcpy
      std::size_t byteSize = computeTypeByteSize(retIrTy);
      emitMemcpy(current_sret_ptr_, v, byteSize);
      current_block_->append<ReturnInst>();
      return;
    }

    auto valPtrTy = std::dynamic_pointer_cast<const PointerType>(v->type());
    if (valPtrTy && isAggregateType(retIrTy)) {
      v = current_block_->append<LoadInst>(v, retIrTy);
    } else {
      v = loadPtrValue(v, retSemTy);
    }

    if (v->type()->isVoid()) {
      current_block_->append<ReturnInst>();
      return;
    } else {
      current_block_->append<ReturnInst>(v);
    }
  } else {
    current_block_->append<ReturnInst>();
  }
}

inline void IREmitter::visit(PrefixExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting prefix operator " + node.op.lexeme);
  // visit, modify, store
  node.right->accept(*this);
  auto operand = popOperand();
  const auto semTy = context->lookupType(&node);
  operand = loadPtrValue(operand, semTy);
  auto irTy = context->resolveType(semTy);

  switch (node.op.type) {
  case TokenType::NOT: {
    auto intTy = std::dynamic_pointer_cast<const IntegerType>(irTy);
    if (!intTy) {
      throw IRException("NOT operator requires integer type operand");
    }
    std::uint64_t allOnes =
        (intTy->bits() == 64) ? ~0ULL : ((1ULL << intTy->bits()) - 1);
    auto lhs = std::make_shared<ConstantInt>(
        std::make_shared<IntegerType>(intTy->bits(), intTy->isSigned()),
        allOnes);
    pushOperand(current_block_->append<BinaryOpInst>(BinaryOpKind::XOR, lhs,
                                                     operand, irTy));
    break;
  }
  case TokenType::MINUS: {
    auto intTy = std::dynamic_pointer_cast<const IntegerType>(irTy);
    auto zeroTy =
        std::make_shared<IntegerType>(intTy->bits(), intTy->isSigned());
    auto zero = std::make_shared<ConstantInt>(zeroTy, 0);
    pushOperand(current_block_->append<BinaryOpInst>(BinaryOpKind::SUB, zero,
                                                     operand, zeroTy));
    break;
  }
  default:
    throw IRException("unsupported prefix operator");
  }
}

inline void IREmitter::visit(LetStatement &node) {
  LOG_DEBUG("[IREmitter] Visiting let statement");
  // alloca & store
  auto *ident = dynamic_cast<IdentifierPattern *>(node.pattern.get());

  node.expr->accept(*this);
  auto init = popOperand();

  auto semTy = ScopeNode::resolve_type(node.type, current_scope_node);
  auto irTy = context->resolveType(semTy);

  auto slot = createAlloca(irTy, ident->name);

  auto initPtrTy = std::dynamic_pointer_cast<const PointerType>(init->type());
  if (initPtrTy && isAggregateType(irTy)) {
    std::size_t byteSize = computeTypeByteSize(irTy);
    emitMemcpy(slot, init, byteSize);
  } else {
    init = loadPtrValue(init, semTy);
    current_block_->append<StoreInst>(init, slot);
  }

  bindLocal(ident->name, slot);
  LOG_DEBUG("[IREmitter] Bound local '" + ident->name + "'");
}

inline void IREmitter::visit(BlockExpression &node) {
  LOG_DEBUG("[IREmitter] Entering block expression");
  auto *previousScope = current_scope_node;
  if (current_scope_node) {
    if (auto *child = current_scope_node->find_child_scope_by_owner(&node)) {
      current_scope_node = child;
    }
  }

  pushLocalScope();

  for (const auto &stmt : node.statements) {
    if (stmt)
      stmt->accept(*this);
  }
  if (node.final_expr) {
    (*node.final_expr)->accept(*this);
  } else {
    pushOperand(std::make_shared<ConstantUnit>());
  }

  popLocalScope();
  current_scope_node = previousScope;
  LOG_DEBUG("[IREmitter] Exiting block expression");
}

inline void IREmitter::visit(LiteralExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting literal: " + node.value);
  const auto semTy = context->lookupType(&node);
  const auto irTy = context->resolveType(semTy);

  if (semTy.is_reference()) {
    const auto &ref = semTy.as_reference();
    if (ref.target->is_primitive() &&
        ref.target->as_primitive().kind == SemPrimitiveKind::STR) {
      auto decoded = decodeStringLiteral(node.value);
      decoded.push_back('\0');
      auto cst = internStringLiteral(decoded);
      pushOperand(cst);
      return;
    }
  }

  if (semTy.is_primitive()) {
    switch (semTy.as_primitive().kind) {
    case SemPrimitiveKind::BOOL: {
      bool value = node.value == "true";
      pushOperand(ConstantInt::getI1(value));
      return;
    }
    case SemPrimitiveKind::CHAR: {
      char ch = decodeCharLiteral(node.value);
      pushOperand(std::make_shared<ConstantInt>(IntegerType::i8(false),
                                                static_cast<std::uint8_t>(ch)));
      return;
    }
    case SemPrimitiveKind::STRING:
    case SemPrimitiveKind::STR: {
      auto decoded = decodeStringLiteral(node.value);
      decoded.push_back('\0');
      auto cst = internStringLiteral(decoded);
      pushOperand(cst);
      return;
    }
    case SemPrimitiveKind::UNIT: {
      pushOperand(std::make_shared<ConstantUnit>());
      return;
    }
    default:
      break;
    }
  }

  // Integer
  auto intTy = std::dynamic_pointer_cast<const IntegerType>(irTy);
  if (!intTy) {
    throw IRException("unsupported literal type");
  }
  std::uint64_t parsed = 0;
  parsed = static_cast<std::uint64_t>(std::stoll(node.value, nullptr, 0));
  pushOperand(std::make_shared<ConstantInt>(
      std::make_shared<IntegerType>(intTy->bits(), intTy->isSigned()), parsed));
}

inline void IREmitter::visit(NameExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting name expression: " + node.name);
  auto value = lookupLocal(node.name);
  if (!value) {
    auto g = globals_.find(node.name);
    if (g != globals_.end()) {
      value = g->second;
      LOG_DEBUG("[IREmitter] Resolved '" + node.name + "' to global");
    }
  } else {
    LOG_DEBUG("[IREmitter] Resolved '" + node.name + "' to local");
  }

  pushOperand(value);
}

inline void IREmitter::visit(ExpressionStatement &node) {
  LOG_DEBUG("[IREmitter] Visiting expression statement");
  if (node.expression)
    node.expression->accept(*this);
  if (node.has_semicolon && !operand_stack_.empty()) {
    LOG_DEBUG("[IREmitter] Discarding expression result due to semicolon");
    operand_stack_.pop_back(); // discard result
  }
}

inline void IREmitter::visit(RootNode &node) {
  LOG_DEBUG("[IREmitter] Visiting root node");
  for (const auto &child : node.children) {
    if (child)
      child->accept(*this);
  }
}

inline void IREmitter::visit(ConstantItem &node) {
  LOG_DEBUG("[IREmitter] Emitting constant: " + node.name);
  auto *item = resolve_value_item(node.name);

  const auto &meta = item->as_constant_meta();

  const auto &cv = *meta.evaluated_value;
  std::shared_ptr<Constant> irConst;

  if (cv.is_bool()) {
    irConst = ConstantInt::getI1(cv.as_bool());
  } else if (cv.is_i32()) {
    irConst =
        ConstantInt::getI32(static_cast<std::uint32_t>(cv.as_i32()), true);
  } else if (cv.is_u32()) {
    irConst = ConstantInt::getI32(cv.as_u32(), false);
  } else if (cv.is_isize()) {
    irConst = std::make_shared<ConstantInt>(
        IntegerType::isize(), static_cast<std::uint64_t>(cv.as_isize()));
  } else if (cv.is_usize()) {
    irConst =
        std::make_shared<ConstantInt>(IntegerType::usize(), cv.as_usize());
  } else if (cv.is_any_int()) {
    // TODO: Treat ANY_INT as i32
    irConst =
        ConstantInt::getI32(static_cast<std::uint32_t>(cv.as_any_int()), true);
  } else if (cv.is_char()) {
    irConst = std::make_shared<ConstantInt>(
        IntegerType::i8(false), static_cast<std::uint8_t>(cv.as_char()));
  } else if (cv.is_string()) {
    auto decoded = decodeStringLiteral(cv.as_string());
    decoded.push_back('\0');
    auto strConst = std::make_shared<ConstantString>(decoded);
    irConst = strConst;
  } else if (cv.type.is_primitive() &&
             cv.type.as_primitive().kind == SemPrimitiveKind::UNIT) {
    irConst = std::make_shared<ConstantUnit>();
  } else {
    throw IRException("constant '" + node.name +
                      "' has unsupported type for IR emission");
  }

  auto mangled = mangle_constant(meta.name, item->owner_scope);
  irConst->setName(mangled);
  module_.createConstant(irConst);
  globals_[meta.name] = irConst;
  LOG_DEBUG("[IREmitter] Registered global constant: " + meta.name +
            " mangled=" + mangled);
}

inline void IREmitter::visit(CallExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting call expression");
  std::vector<ValuePtr> args;
  args.reserve(node.arguments.size());
  for (const auto &arg : node.arguments) {
    if (arg) {
      arg->accept(*this);
      args.push_back(popOperand());
    }
  }

  const FunctionMetaData *meta = nullptr;
  const ScopeNode *owner_scope = current_scope_node;
  std::optional<std::string> member_of;

  if (auto *nameExpr =
          dynamic_cast<NameExpression *>(node.function_name.get())) {
    auto *item = resolve_value_item(nameExpr->name);
    if (!item || !item->has_function_meta()) {
      LOG_ERROR("[IREmitter] unknown function '" + nameExpr->name + "'");
      throw IRException("unknown function '" + nameExpr->name + "'");
    }
    meta = &item->as_function_meta();
    owner_scope = item->owner_scope;
  } else if (auto *pathExpr =
                 dynamic_cast<PathExpression *>(node.function_name.get())) {
    const std::string &type_name = pathExpr->segments[0].ident;
    const std::string &fn_name = pathExpr->segments[1].ident;
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

  auto paramSems = build_effective_params(*meta); // handle &self

  std::vector<TypePtr> irParams;
  irParams.reserve(paramSems.size());
  for (const auto &p : paramSems) {
    irParams.push_back(context->resolveType(p));
  }
  auto originalRetTy = context->resolveType(meta->return_type);
  auto retTy = originalRetTy;

  bool needSret = isAggregateType(originalRetTy);
  if (needSret) {
    auto sretPtrTy = std::make_shared<PointerType>(originalRetTy);
    irParams.insert(irParams.begin(), sretPtrTy);
    retTy = std::make_shared<VoidType>();
  }

  auto found = find_function(meta);
  if (!found) { // this happens for builtins or forward declarations
    LOG_DEBUG("[IREmitter] Predeclaring function '" + meta->name + "'");
    auto fnTy = std::make_shared<FunctionType>(retTy, irParams, false);
    auto mangled = mangle_function(*meta, owner_scope, member_of);
    found = create_function(mangled, fnTy,
                            !meta->decl || !meta->decl->body.has_value(), meta);
    if (needSret) {
      sret_functions_[meta] = originalRetTy;
    }
  }

  auto callee = function_symbol(*meta, found);

  std::vector<ValuePtr> resolved;

  // For sret functions, create alloca
  ValuePtr sretSlot;
  if (needSret) {
    sretSlot = createAlloca(originalRetTy, "sret_result");
    resolved.push_back(sretSlot);
  }

  resolved.reserve(args.size() + (needSret ? 1 : 0));
  for (size_t i = 0; i < args.size(); ++i) {
    resolved.push_back(
        resolve_ptr(args[i], paramSems[i], "arg" + std::to_string(i)));
  }

  LOG_DEBUG("[IREmitter] Emitting call to function " + meta->name);
  auto callInst = current_block_->append<CallInst>(callee, resolved, retTy);

  if (needSret) {
    // For sret, the result is already in sretSlot
    pushOperand(sretSlot);
  } else {
    pushOperand(callInst);
  }
}

inline void IREmitter::visit(StructDecl &node) {
  LOG_DEBUG("[IREmitter] Visiting struct declaration: " + node.name);
  auto *item = current_scope_node
                   ? current_scope_node->find_type_item(node.name)
                   : nullptr;
  const auto &meta = item->as_struct_meta();
  std::vector<std::pair<std::string, TypePtr>> fields;
  for (const auto &[fieldName, fieldType] : meta.named_fields) {
    auto irTy = context->resolveType(fieldType);
    fields.push_back(std::make_pair(fieldName, irTy));
  }

  auto mangled = mangle_struct(*item);
  name_mangle_[&node] = mangled;
  auto structType = module_.createStructType(fields, mangled);
  struct_types_.push_back(structType);
  LOG_INFO("[IREmitter] Created struct type: " + node.name +
           " mangled=" + mangled);

  // Emit evaluated associated constants
  for (const auto &c : meta.constants) {
    if (!c.evaluated_value) {
      continue;
    }
    const auto &cv = *c.evaluated_value;
    std::shared_ptr<Constant> irConst;
    if (cv.is_bool()) {
      irConst = ConstantInt::getI1(cv.as_bool());
    } else if (cv.is_i32()) {
      irConst =
          ConstantInt::getI32(static_cast<std::uint32_t>(cv.as_i32()), true);
    } else if (cv.is_u32()) {
      irConst = ConstantInt::getI32(cv.as_u32(), false);
    } else if (cv.is_isize()) {
      irConst = std::make_shared<ConstantInt>(
          IntegerType::isize(), static_cast<std::uint64_t>(cv.as_isize()));
    } else if (cv.is_usize()) {
      irConst =
          std::make_shared<ConstantInt>(IntegerType::usize(), cv.as_usize());
    } else if (cv.is_any_int()) {
      irConst = ConstantInt::getI32(static_cast<std::uint32_t>(cv.as_any_int()),
                                    true);
    } else if (cv.is_char()) {
      irConst = std::make_shared<ConstantInt>(
          IntegerType::i8(false), static_cast<std::uint8_t>(cv.as_char()));
    } else if (cv.is_string()) {
      auto decoded = decodeStringLiteral(cv.as_string());
      decoded.push_back('\0');
      irConst = std::make_shared<ConstantString>(decoded);
    } else if (cv.type.is_primitive() &&
               cv.type.as_primitive().kind == SemPrimitiveKind::UNIT) {
      irConst = std::make_shared<ConstantUnit>();
    } else {
      continue;
    }
    auto mangled_const = mangle_constant(c.name, item->owner_scope, item->name);
    irConst->setName(mangled_const);
    module_.createConstant(irConst);
    globals_[c.name] = irConst;
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
    std::vector<TypePtr> irParams;
    irParams.reserve(params.size());
    for (const auto &p : params) {
      irParams.push_back(context->resolveType(p));
    }
    auto originalRetTy = context->resolveType(m.return_type);
    auto retTy = originalRetTy;

    // Check if this method should use sret
    bool needSret = isAggregateType(originalRetTy);
    if (needSret) {
      auto sretPtrTy = std::make_shared<PointerType>(originalRetTy);
      irParams.insert(irParams.begin(), sretPtrTy);
      retTy = std::make_shared<VoidType>();
      sret_functions_[&m] = originalRetTy;
    }

    auto fnTy = std::make_shared<FunctionType>(retTy, irParams, false);
    auto mangled_fn = mangle_function(m, item->owner_scope, item->name);
    create_function(mangled_fn, fnTy, !m.decl || !m.decl->body.has_value(), &m);
    LOG_DEBUG("[IREmitter] Precreated method function: " + m.name +
              " mangled=" + mangled_fn);
  }

  return;
}

inline void IREmitter::visit(EnumDecl &) {
  throw std::runtime_error("EnumDecl emission not implemented");
}

inline void IREmitter::visit(TraitDecl &) {
  throw std::runtime_error("TraitDecl emission not implemented");
}

inline void IREmitter::visit(ImplDecl &node) {
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

inline void IREmitter::visit(EmptyStatement &) {}

inline void IREmitter::visit(GroupExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting grouped expression");
  if (node.inner)
    node.inner->accept(*this);
}

inline void IREmitter::visit(IfExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting if expression");
  node.condition->accept(*this);
  auto condVal = popOperand();
  condVal = loadPtrValue(condVal, context->lookupType(node.condition.get()));

  auto cur_func = functions_.back();
  auto thenBlock = cur_func->createBlock("if_then");
  auto resultSemTy = context->lookupType(&node);
  auto resultIrTy = context->resolveType(resultSemTy);
  auto entryBlock = current_block_;

  bool useResultSlot = isAggregateType(resultIrTy);
  ValuePtr resultSlot;
  if (useResultSlot) {
    resultSlot = createAlloca(resultIrTy, "if_result");
  }

  if (node.else_block) {
    auto elseBlock = cur_func->createBlock("if_else");
    auto mergeBlock = cur_func->createBlock("if_merge");
    current_block_->append<BranchInst>(condVal, thenBlock, elseBlock);
    // then block
    current_block_ = thenBlock;
    if (node.then_block)
      node.then_block->accept(*this);
    auto thenVal = popOperand();
    auto thenTerminated = current_block_->isTerminated();
    auto thenExitBlock = current_block_;
    if (!thenTerminated) {
      if (useResultSlot) {
        auto thenPtrTy =
            std::dynamic_pointer_cast<const PointerType>(thenVal->type());
        std::size_t byteSize = computeTypeByteSize(resultIrTy);
        emitMemcpy(resultSlot, thenVal, byteSize);
      } else {
        thenVal = loadPtrValue(thenVal, resultSemTy);
      }
      current_block_->append<BranchInst>(mergeBlock);
    }
    // else block
    current_block_ = elseBlock;
    if (node.else_block)
      std::static_pointer_cast<BlockExpression>(node.else_block.value())
          ->accept(*this);
    auto elseVal = popOperand();
    auto elseTerminated = current_block_->isTerminated();
    auto elseExitBlock = current_block_;
    if (!elseTerminated) {
      if (useResultSlot) {
        auto elsePtrTy =
            std::dynamic_pointer_cast<const PointerType>(elseVal->type());
        std::size_t byteSize = computeTypeByteSize(resultIrTy);
        emitMemcpy(resultSlot, elseVal, byteSize);
      } else {
        elseVal = loadPtrValue(elseVal, resultSemTy);
      }
      current_block_->append<BranchInst>(mergeBlock);
    }
    // merge block
    current_block_ = mergeBlock;
    if (thenTerminated && elseTerminated) {
      current_block_->append<UnreachableInst>();
      pushOperand(std::make_shared<ConstantUnit>());
      return;
    }
    if (resultIrTy->isVoid()) {
      pushOperand(std::make_shared<ConstantUnit>());
      return;
    }

    if (useResultSlot) {
      pushOperand(resultSlot);
    } else {
      std::vector<PhiInst::Incoming> incomings;
      if (!thenTerminated) {
        incomings.push_back({thenVal, thenExitBlock});
      }
      if (!elseTerminated) {
        incomings.push_back({elseVal, elseExitBlock});
      }
      if (incomings.empty()) {
        current_block_->append<UnreachableInst>();
        pushOperand(std::make_shared<ConstantUnit>());
        return;
      }
      auto phi =
          current_block_->append<PhiInst>(resultIrTy, incomings, "ifval");
      pushOperand(phi);
    }
  } else {
    auto mergeBlock = cur_func->createBlock("if_merge");
    current_block_->append<BranchInst>(condVal, thenBlock, mergeBlock);
    // then block
    current_block_ = thenBlock;
    if (node.then_block)
      node.then_block->accept(*this);
    auto thenVal = popOperand();
    auto thenTerminated = current_block_->isTerminated();
    auto thenExitBlock = current_block_;
    if (!thenTerminated) {
      if (useResultSlot) {
        auto thenPtrTy =
            std::dynamic_pointer_cast<const PointerType>(thenVal->type());
        std::size_t byteSize = computeTypeByteSize(resultIrTy);
        emitMemcpy(resultSlot, thenVal, byteSize);
      } else {
        thenVal = loadPtrValue(thenVal, resultSemTy);
      }
      current_block_->append<BranchInst>(mergeBlock);
    }
    // merge block
    current_block_ = mergeBlock;
    auto unit = std::make_shared<ConstantUnit>();
    if (resultIrTy->isVoid()) {
      pushOperand(unit);
      return;
    }

    if (useResultSlot) {
      pushOperand(resultSlot);
    } else {
      std::vector<PhiInst::Incoming> incomings;
      if (!thenTerminated) {
        incomings.push_back({thenVal, thenExitBlock});
      }
      incomings.push_back({unit, entryBlock});
      auto phi =
          current_block_->append<PhiInst>(resultIrTy, incomings, "ifval");
      pushOperand(phi);
    }
  }
}

inline void IREmitter::visit(MethodCallExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting method call: " + node.method_name.name);

  node.receiver->accept(*this);
  auto receiverVal = popOperand();
  auto recvSem = context->lookupType(node.receiver.get());
  SemType lookupType = recvSem;
  if (lookupType.is_reference()) {
    lookupType = *lookupType.as_reference().target;
  }

  const CollectedItem *ci = lookupType.as_named().item;

  const auto &meta = ci->as_struct_meta();
  const FunctionMetaData *found = nullptr;
  for (const auto &m : meta.methods) {
    if (m.name == node.method_name.name) {
      found = &m;
      break;
    }
  }

  std::optional<SemType> selfSem;
  if (found->decl && found->decl->self_param) {
    selfSem = compute_self_type(*found->decl, ci);
  }

  auto paramSems = build_effective_params(*found, selfSem);

  std::vector<TypePtr> paramIr;
  paramIr.reserve(paramSems.size());
  for (const auto &p : paramSems) {
    paramIr.push_back(context->resolveType(p));
  }
  auto originalRetTy = context->resolveType(found->return_type);
  auto retTy = originalRetTy;

  bool needSret = isAggregateType(originalRetTy);
  if (needSret) {
    auto sretPtrTy = std::make_shared<PointerType>(originalRetTy);
    paramIr.insert(paramIr.begin(), sretPtrTy);
    retTy = std::make_shared<VoidType>();
  }

  auto foundFn = find_function(found);
  if (!foundFn) {
    auto fnTy = std::make_shared<FunctionType>(retTy, paramIr, false);
    auto mangled = mangle_function(*found, ci->owner_scope, ci->name);
    foundFn = create_function(
        mangled, fnTy, !found->decl || !found->decl->body.has_value(), found);
    if (needSret) {
      sret_functions_[found] = originalRetTy;
    }
    LOG_DEBUG("[IREmitter] Predeclared method function: " + found->name +
              " mangled=" + mangled);
  }
  auto callee = function_symbol(*found, foundFn);

  std::vector<ValuePtr> args;
  args.reserve(paramSems.size() + (needSret ? 1 : 0));

  ValuePtr sretSlot;
  if (needSret) {
    sretSlot = createAlloca(originalRetTy, "sret_result");
    args.push_back(sretSlot);
  }

  size_t argIndex = 0;

  if (selfSem) {
    args.push_back(resolve_ptr(receiverVal, *selfSem, "self"));
    argIndex = 1;
  }

  for (size_t i = 0; i < node.arguments.size(); ++i) {
    if (node.arguments[i]) {
      node.arguments[i]->accept(*this);
      auto v = popOperand();
      args.push_back(resolve_ptr(v, paramSems[argIndex + i],
                                 "arg" + std::to_string(argIndex + i)));
    }
  }

  auto call = current_block_->append<CallInst>(callee, args, retTy);

  if (needSret) {
    pushOperand(sretSlot);
  } else {
    pushOperand(call);
  }
  LOG_DEBUG("[IREmitter] Emitted method call to " + found->name);
}

inline void IREmitter::visit(FieldAccessExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting field access: " + node.field_name);
  node.target->accept(*this);
  auto targetVal = popOperand();

  auto targetType = context->lookupType(node.target.get());
  if (targetType.is_reference()) {
    targetType = *targetType.as_reference().target;
  }

  const CollectedItem *ci = targetType.as_named().item;
  const auto &meta = ci->as_struct_meta();
  size_t index = 0;
  SemType fieldSem;
  for (size_t i = 0; i < meta.named_fields.size(); ++i) {
    if (meta.named_fields[i].first == node.field_name) {
      index = i;
      fieldSem = meta.named_fields[i].second;
      break;
    }
  }

  auto structIrTy = context->resolveType(SemType::named(ci));
  auto structPtr =
      std::dynamic_pointer_cast<const PointerType>(targetVal->type())
          ? targetVal
          : nullptr;
  if (!structPtr) {
    LOG_ERROR("[IREmitter] field access target is not addressable");
    throw IRException("field access target is not addressable");
  }

  auto structPtrTy =
      std::dynamic_pointer_cast<const PointerType>(structPtr->type());
  while (structPtrTy && !typeEquals(structPtrTy->pointee(), structIrTy)) {
    structPtr =
        current_block_->append<LoadInst>(structPtr, structPtrTy->pointee());
    structPtrTy =
        std::dynamic_pointer_cast<const PointerType>(structPtr->type());
  }

  auto zero = ConstantInt::getI32(0, false);
  auto idxConst = ConstantInt::getI32(static_cast<std::uint32_t>(index), false);
  auto fieldIrTy = context->resolveType(fieldSem);
  auto gep = current_block_->append<GetElementPtrInst>(
      fieldIrTy, structPtr, std::vector<ValuePtr>{zero, idxConst},
      node.field_name);

  // auto loaded = current_block_->append<LoadInst>(gep, fieldIrTy, 0, false,
  // node.field_name);
  pushOperand(gep);
  LOG_DEBUG("[IREmitter] Loaded field '" + node.field_name + "'");
}

inline void IREmitter::visit(StructExpression &node) {
  LOG_DEBUG("[IREmitter] Emitting struct expression");

  auto *nameExpr = dynamic_cast<NameExpression *>(node.path_expr.get());

  const auto *item = resolve_struct_item(nameExpr->name);
  const auto &meta = item->as_struct_meta();

  std::unordered_map<std::string, std::shared_ptr<Expression>> provided;
  for (const auto &f : node.fields) {
    provided[f.name] = f.value.value();
  }

  auto structSem = SemType::named(item);
  auto structIrTy = context->resolveType(structSem);
  auto slot = createAlloca(structIrTy, "structtmp");

  auto zero = ConstantInt::getI32(0, false);
  for (size_t i = 0; i < meta.named_fields.size(); ++i) {
    const auto &field = meta.named_fields[i];
    auto it = provided.find(field.first);
    if (it == provided.end()) {
      throw IRException("missing initializer for field '" + field.first + "'");
    }
    it->second->accept(*this);
    auto val = popOperand();
    auto idxConst = ConstantInt::getI32(static_cast<std::uint32_t>(i), false);
    auto fieldTy = context->resolveType(field.second);
    auto gep = current_block_->append<GetElementPtrInst>(
        fieldTy, slot, std::vector<ValuePtr>{zero, idxConst}, field.first);

    // For aggregate fields, use memcpy instead of load+store
    auto valPtrTy = std::dynamic_pointer_cast<const PointerType>(val->type());
    if (valPtrTy && isAggregateType(fieldTy)) {
      std::size_t byteSize = computeTypeByteSize(fieldTy);
      emitMemcpy(gep, val, byteSize);
    } else {
      auto resolved = resolve_ptr(val, field.second, field.first);
      current_block_->append<StoreInst>(resolved, gep);
    }
  }

  pushOperand(slot);
  LOG_DEBUG("[IREmitter] Constructed struct instance for " + nameExpr->name);
}

inline void IREmitter::visit(UnderscoreExpression &) {
  LOG_DEBUG("[IREmitter] Visiting underscore expression (unit)");
  pushOperand(std::make_shared<ConstantUnit>());
}

inline void IREmitter::visit(LoopExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting loop expression");
  auto cur_func = functions_.back();
  auto bodyBlock = cur_func->createBlock("loop_body");
  auto afterBlock = cur_func->createBlock("loop_after");

  current_block_->append<BranchInst>(bodyBlock);

  current_block_ = bodyBlock;
  // break -> afterBlock, continue -> bodyBlock
  loop_stack_.push_back({afterBlock, bodyBlock});
  node.body->accept(*this);
  if (!current_block_->isTerminated()) {
    current_block_->append<BranchInst>(bodyBlock);
  }
  loop_stack_.pop_back();

  current_block_ = afterBlock;
  pushOperand(std::make_shared<ConstantUnit>());
  LOG_DEBUG("[IREmitter] Finished loop expression");
}

inline void IREmitter::visit(WhileExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting while expression");
  auto cur_func = functions_.back();
  auto condBlock = cur_func->createBlock("while_cond");
  auto bodyBlock = cur_func->createBlock("while_body");
  auto afterBlock = cur_func->createBlock("while_after");

  current_block_->append<BranchInst>(condBlock);

  current_block_ = condBlock;
  node.condition->accept(*this);
  auto condVal = popOperand();
  condVal = loadPtrValue(condVal, context->lookupType(node.condition.get()));
  current_block_->append<BranchInst>(condVal, bodyBlock, afterBlock);

  current_block_ = bodyBlock;
  // break -> afterBlock, continue -> condBlock
  loop_stack_.push_back({afterBlock, condBlock});
  node.body->accept(*this);
  if (!current_block_->isTerminated()) {
    current_block_->append<BranchInst>(condBlock);
  }
  loop_stack_.pop_back();

  current_block_ = afterBlock;
  pushOperand(std::make_shared<ConstantUnit>());
  LOG_DEBUG("[IREmitter] Finished while expression");
}

inline void IREmitter::visit(ArrayExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting array expression");

  auto semTy = context->lookupType(&node);
  const auto &arrSem = semTy.as_array();
  std::size_t count = static_cast<std::size_t>(arrSem.size);
  if (node.actual_size >= 0) {
    count = static_cast<std::size_t>(node.actual_size);
  }

  auto arrIrTy = context->resolveType(semTy);
  auto elemIrTy = context->resolveType(*arrSem.element);
  auto slot = createAlloca(arrIrTy, "arrtmp");
  auto zero = ConstantInt::getI32(0, false);

  if (node.repeat) {
    if (isZeroLiteral(node.repeat->first.get())) {
      std::size_t byteSize = computeTypeByteSize(arrIrTy);
      LOG_DEBUG("[IREmitter] Using memset optimization for zero-initialized "
                "array of " +
                std::to_string(byteSize) + " bytes");
      auto memsetFn = memset_fn_;
      auto ptrTy = std::make_shared<PointerType>(IntegerType::i32(false));
      // memset(dest, value=0, size)
      std::vector<ValuePtr> args;
      args.push_back(slot);
      args.push_back(ConstantInt::getI32(0, true));
      args.push_back(
          ConstantInt::getI32(static_cast<std::uint32_t>(byteSize), false));

      auto memsetSymbol =
          std::make_shared<Value>(memsetFn->type(), memsetFn->name());
      current_block_->append<CallInst>(memsetSymbol, args, ptrTy);
    } else {
      node.repeat->first->accept(*this);
      auto val = popOperand();

      auto valPtrTy = std::dynamic_pointer_cast<const PointerType>(val->type());
      bool elemIsAggregate = isAggregateType(elemIrTy);

      std::size_t repeatCount = count;
      if (elemIsAggregate && valPtrTy) {
        // Use memcpy for each element
        std::size_t elemByteSize = computeTypeByteSize(elemIrTy);
        for (std::size_t i = 0; i < repeatCount; ++i) {
          auto idxConst =
              ConstantInt::getI32(static_cast<std::uint32_t>(i), false);
          auto gep = current_block_->append<GetElementPtrInst>(
              elemIrTy, slot, std::vector<ValuePtr>{zero, idxConst}, "elt");
          emitMemcpy(gep, val, elemByteSize);
        }
      } else {
        auto resolved = resolve_ptr(val, *arrSem.element, "arr_init");
        for (std::size_t i = 0; i < repeatCount; ++i) {
          auto idxConst =
              ConstantInt::getI32(static_cast<std::uint32_t>(i), false);
          auto gep = current_block_->append<GetElementPtrInst>(
              elemIrTy, slot, std::vector<ValuePtr>{zero, idxConst}, "elt");
          current_block_->append<StoreInst>(resolved, gep);
        }
      }
    }
  } else {
    bool elemIsAggregate = isAggregateType(elemIrTy);
    std::size_t elemByteSize =
        elemIsAggregate ? computeTypeByteSize(elemIrTy) : 0;

    for (std::size_t i = 0; i < node.elements.size(); ++i) {
      node.elements[i]->accept(*this);
      auto val = popOperand();
      auto idxConst = ConstantInt::getI32(static_cast<std::uint32_t>(i), false);
      auto gep = current_block_->append<GetElementPtrInst>(
          elemIrTy, slot, std::vector<ValuePtr>{zero, idxConst}, "elt");

      auto valPtrTy = std::dynamic_pointer_cast<const PointerType>(val->type());
      if (elemIsAggregate && valPtrTy) {
        emitMemcpy(gep, val, elemByteSize);
      } else {
        auto resolved =
            resolve_ptr(val, *arrSem.element, "elt" + std::to_string(i));
        current_block_->append<StoreInst>(resolved, gep);
      }
    }
  }

  pushOperand(slot);
  LOG_DEBUG("[IREmitter] Constructed array of count " + std::to_string(count));
}

inline void IREmitter::visit(IndexExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting index expression");

  node.target->accept(*this);
  auto targetVal = popOperand();
  auto targetSem = context->lookupType(node.target.get());
  if (targetSem.is_reference()) {
    targetSem = *targetSem.as_reference().target;
  }

  const auto &arrSem = targetSem.as_array();
  auto elemIrTy = context->resolveType(*arrSem.element);
  ValuePtr basePtr;
  if (auto ptrTy =
          std::dynamic_pointer_cast<const PointerType>(targetVal->type())) {
    auto pointee = ptrTy->pointee();
    while (ptrTy && !std::dynamic_pointer_cast<const ArrayType>(pointee)) {
      targetVal = current_block_->append<LoadInst>(targetVal, pointee);
      ptrTy = std::dynamic_pointer_cast<const PointerType>(targetVal->type());
      if (!ptrTy) {
        break;
      }
      pointee = ptrTy->pointee();
    }
    if (ptrTy && std::dynamic_pointer_cast<const ArrayType>(ptrTy->pointee())) {
      basePtr = targetVal;
    }
  } else {
    throw IRException("array target is not addressable");
  }

  node.index->accept(*this);
  auto idxVal = popOperand();
  auto idxTy = std::dynamic_pointer_cast<const IntegerType>(idxVal->type());
  if (!idxTy) {
    auto loadedIdx =
        loadPtrValue(idxVal, context->lookupType(node.index.get()));
    idxVal = loadedIdx;
    if (!std::dynamic_pointer_cast<const IntegerType>(idxVal->type())) {
      LOG_ERROR("[IREmitter] array index must be integer");
      throw IRException("array index must be integer");
    }
  }

  auto zero = ConstantInt::getI32(0, false);
  auto gep = current_block_->append<GetElementPtrInst>(
      elemIrTy, basePtr, std::vector<ValuePtr>{zero, idxVal}, "idx");
  pushOperand(gep);
  LOG_DEBUG("[IREmitter] Emitted index access");
}

inline void IREmitter::visit(TupleExpression &) {
  LOG_ERROR("[IREmitter] TupleExpression emission not implemented");
  throw std::runtime_error("TupleExpression emission not implemented");
}

inline void IREmitter::visit(BreakExpression &) {
  LOG_DEBUG("[IREmitter] Visiting break expression");
  if (loop_stack_.empty()) {
    throw IRException("break used outside of loop"); // this should never happen
  }
  // break target is the first element of the pair
  auto targetBlock = loop_stack_.back().first;
  pushOperand(std::make_shared<ConstantUnit>());
  current_block_->append<BranchInst>(targetBlock);
  // should never have dead code, but just in case
  auto cur_func = functions_.back();
  auto unreachableBlock = cur_func->createBlock("unreachable_after_break");
  current_block_ = unreachableBlock;
}

inline void IREmitter::visit(ContinueExpression &) {
  LOG_DEBUG("[IREmitter] Visiting continue expression");
  if (loop_stack_.empty()) {
    throw IRException(
        "continue used outside of loop"); // this should never happen
  }
  auto targetBlock = loop_stack_.back().second;
  pushOperand(std::make_shared<ConstantUnit>());
  current_block_->append<BranchInst>(targetBlock);
  // should never have dead code, but just in case
  auto cur_func = functions_.back();
  auto unreachableBlock = cur_func->createBlock("unreachable_after_continue");
  current_block_ = unreachableBlock;
}

inline void IREmitter::visit(PathExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting path expression");

  if (node.segments.size() == 1) {
    const auto &ident = node.segments[0].ident;
    if (auto local = lookupLocal(ident)) {
      LOG_DEBUG("[IREmitter] Path resolved to local '" + ident + "'");
      pushOperand(local);
      return;
    }
    auto g = globals_.find(ident);
    if (g != globals_.end()) {
      LOG_DEBUG("[IREmitter] Path resolved to global '" + ident + "'");
      pushOperand(g->second);
      return;
    }
    throw IRException("identifier '" + ident + "' not found");
  }

  if (node.segments.size() != 2) {
    throw IRException("only TypeName::Value paths are supported");
  }

  const std::string &typeName = node.segments[0].ident;
  const std::string &valName = node.segments[1].ident;

  const CollectedItem *type_item = nullptr;
  if (typeName == "Self" && current_impl_target_) {
    type_item = current_impl_target_;
  } else {
    type_item = resolve_struct_item(typeName);
  }

  const ConstantMetaData *foundConst = nullptr;
  for (const auto &c : type_item->as_struct_meta().constants) {
    if (c.name == valName) {
      foundConst = &c;
      break;
    }
  }

  if (!foundConst || !foundConst->evaluated_value) {
    throw IRException("associated constant '" + valName +
                      "' not found or unevaluated");
  }

  // reuse existing emitted constant
  auto mangled =
      mangle_constant(valName, type_item->owner_scope, type_item->name);
  for (const auto &c : module_.constants()) {
    if (c && c->name() == mangled) {
      globals_[valName] = c;
      pushOperand(c);
      LOG_DEBUG("[IREmitter] Resolved path to existing constant " + mangled);
      return;
    }
  }

  const auto &cv = *foundConst->evaluated_value;
  std::shared_ptr<Constant> irConst;
  if (cv.is_bool()) {
    irConst = ConstantInt::getI1(cv.as_bool());
  } else if (cv.is_i32()) {
    irConst =
        ConstantInt::getI32(static_cast<std::uint32_t>(cv.as_i32()), true);
  } else if (cv.is_u32()) {
    irConst = ConstantInt::getI32(cv.as_u32(), false);
  } else if (cv.is_isize()) {
    irConst = std::make_shared<ConstantInt>(
        IntegerType::isize(), static_cast<std::uint64_t>(cv.as_isize()));
  } else if (cv.is_usize()) {
    irConst =
        std::make_shared<ConstantInt>(IntegerType::usize(), cv.as_usize());
  } else if (cv.is_any_int()) {
    irConst =
        ConstantInt::getI32(static_cast<std::uint32_t>(cv.as_any_int()), true);
  } else if (cv.is_char()) {
    irConst = std::make_shared<ConstantInt>(
        IntegerType::i8(false), static_cast<std::uint8_t>(cv.as_char()));
  } else if (cv.is_string()) {
    auto decoded = decodeStringLiteral(cv.as_string());
    decoded.push_back('\0');
    irConst = std::make_shared<ConstantString>(decoded);
  } else if (cv.type.is_primitive() &&
             cv.type.as_primitive().kind == SemPrimitiveKind::UNIT) {
    irConst = std::make_shared<ConstantUnit>();
  } else {
    throw IRException("unsupported associated constant type for '" + valName +
                      "'");
  }

  irConst->setName(mangled);
  module_.createConstant(irConst);
  globals_[valName] = irConst;
  pushOperand(irConst);
  LOG_DEBUG("[IREmitter] Created path constant " + mangled);
}

inline void IREmitter::visit(QualifiedPathExpression &) {
  LOG_ERROR("[IREmitter] QualifiedPathExpression emission not implemented");
  throw std::runtime_error("QualifiedPathExpression emission not implemented");
}

inline void IREmitter::visit(BorrowExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting borrow expression");

  node.right->accept(*this);
  auto value = popOperand();

  auto targetSem = context->lookupType(node.right.get());
  auto targetIrTy = context->resolveType(targetSem);

  if (auto ptrTy =
          std::dynamic_pointer_cast<const PointerType>(value->type())) {
    LOG_DEBUG("[IREmitter] borrow returning existing pointer");
    pushOperand(value);
    return;
  } else {
    throw IRException("not available for borrow");
  }
}

inline void IREmitter::visit(DerefExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting deref expression");
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block for deref");
    throw IRException("no active basic block");
  }

  node.right->accept(*this);
  auto ptrVal = popOperand();

  auto rightSem = context->lookupType(node.right.get());
  auto resultSem = context->lookupType(&node);
  auto resultIrTy = context->resolveType(resultSem);

  auto current = ptrVal;
  for (int i = 0; i < 8; ++i) { // we only deref up to 8 levels deep
    auto curPtrTy =
        std::dynamic_pointer_cast<const PointerType>(current->type());
    if (typeEquals(curPtrTy->pointee(), resultIrTy)) {
      break;
    }
    auto innerPtr =
        std::dynamic_pointer_cast<const PointerType>(curPtrTy->pointee());
    if (!innerPtr) {
      LOG_ERROR("[IREmitter] deref result pointer type mismatch");
      throw IRException("deref result pointer type mismatch");
    }
    current = current_block_->append<LoadInst>(current, curPtrTy->pointee());
  }

  pushOperand(current);
}

inline ValuePtr IREmitter::popOperand() {
  if (operand_stack_.empty()) {
    LOG_ERROR("[IREmitter] operand stack underflow");
    throw IRException("operand stack underflow");
  }
  auto v = operand_stack_.back();
  operand_stack_.pop_back();
  LOG_DEBUG("[IREmitter] popOperand -> " + (v ? v->name() : "<null>"));
  return v;
}

inline void IREmitter::pushOperand(ValuePtr v) {
  if (!v) {
    LOG_ERROR("[IREmitter] attempt to push null operand");
    throw IRException("attempt to push null operand");
  }
  LOG_DEBUG("[IREmitter] pushOperand -> " + v->name());
  operand_stack_.push_back(std::move(v));
}

inline ValuePtr IREmitter::createAlloca(const TypePtr &ty,
                                        const std::string &name,
                                        ValuePtr arraySize,
                                        unsigned alignment) {
  return current_entry_block_->prepend<AllocaInst>(ty, std::move(arraySize),
                                                   alignment, name);
}

inline ValuePtr IREmitter::loadPtrValue(ValuePtr v, const SemType &semTy) {
  if (!v) {
    throw IRException("attempt to materialize null value");
  }
  auto ptrTy = std::dynamic_pointer_cast<const PointerType>(v->type());
  if (!ptrTy) {
    return v;
  }

  if (!semTy.is_reference()) {
    // For aggregate types, DON'T load them
    if (isAggregateType(ptrTy->pointee())) {
      LOG_DEBUG("[IREmitter] loadPtrValue: keeping aggregate as pointer");
      return v;
    }
    LOG_DEBUG("[IREmitter] loadPtrValue loading from pointer");
    return current_block_->append<LoadInst>(v, ptrTy->pointee());
  }

  auto expectedTy = context->resolveType(semTy);
  if (typeEquals(v->type(), expectedTy)) {
    return v;
  }

  auto current = v;
  auto currentPtrTy = ptrTy;
  while (currentPtrTy && !typeEquals(current->type(), expectedTy)) {
    if (isAggregateType(currentPtrTy->pointee())) {
      // don't load aggregates
      LOG_DEBUG("[IREmitter] loadPtrValue: stopping at aggregate pointer");
      return current;
    }
    current =
        current_block_->append<LoadInst>(current, currentPtrTy->pointee());
    currentPtrTy =
        std::dynamic_pointer_cast<const PointerType>(current->type());
  }

  if (typeEquals(current->type(), expectedTy)) {
    LOG_DEBUG("[IREmitter] loadPtrValue peeled pointer for reference");
    return current;
  }

  throw IRException("loadPtrValue unable to match reference type");
}

inline bool IREmitter::typeEquals(const TypePtr &a, const TypePtr &b) const {
  if (a->kind() != b->kind())
    return false;
  switch (a->kind()) {
  case TypeKind::Void:
    return true;
  case TypeKind::Integer: {
    auto ia = std::static_pointer_cast<const IntegerType>(a);
    auto ib = std::static_pointer_cast<const IntegerType>(b);
    return ia->bits() == ib->bits() && ia->isSigned() == ib->isSigned();
  }
  case TypeKind::Pointer: {
    auto pa = std::static_pointer_cast<const PointerType>(a);
    auto pb = std::static_pointer_cast<const PointerType>(b);
    return typeEquals(pa->pointee(), pb->pointee());
  }
  case TypeKind::Array: {
    auto aa = std::static_pointer_cast<const ArrayType>(a);
    auto ab = std::static_pointer_cast<const ArrayType>(b);
    return aa->count() == ab->count() && typeEquals(aa->elem(), ab->elem());
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
      if (!typeEquals(sa->fields()[i], sb->fields()[i]))
        return false;
    }
    return true;
  }
  case TypeKind::Function: {
    auto fa = std::static_pointer_cast<const FunctionType>(a);
    auto fb = std::static_pointer_cast<const FunctionType>(b);
    if (fa->isVarArg() != fb->isVarArg())
      return false;
    if (!typeEquals(fa->returnType(), fb->returnType()))
      return false;
    if (fa->paramTypes().size() != fb->paramTypes().size())
      return false;
    for (size_t i = 0; i < fa->paramTypes().size(); ++i) {
      if (!typeEquals(fa->paramTypes()[i], fb->paramTypes()[i]))
        return false;
    }
    return true;
  }
  }
  return false;
}

inline ValuePtr IREmitter::lookupLocal(const std::string &name) const {
  for (auto it = locals_.rbegin(); it != locals_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return nullptr;
}

inline void IREmitter::bindLocal(const std::string &name, ValuePtr v) {
  if (locals_.empty()) {
    locals_.emplace_back();
  }
  locals_.back()[name] = std::move(v);
  LOG_DEBUG("[IREmitter] bindLocal: " + name);
}

inline void IREmitter::pushLocalScope() {
  locals_.emplace_back();
  LOG_DEBUG("[IREmitter] pushLocalScope depth=" +
            std::to_string(locals_.size()));
}

inline void IREmitter::popLocalScope() {
  if (!locals_.empty())
    locals_.pop_back();
  LOG_DEBUG("[IREmitter] popLocalScope depth=" +
            std::to_string(locals_.size()));
}

inline bool IREmitter::isAssignment(TokenType tt) const {
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

inline bool IREmitter::isInteger(SemPrimitiveKind k) const {
  return k == SemPrimitiveKind::ANY_INT || k == SemPrimitiveKind::I32 ||
         k == SemPrimitiveKind::U32 || k == SemPrimitiveKind::ISIZE ||
         k == SemPrimitiveKind::USIZE;
}

inline std::optional<BinaryOpKind> IREmitter::tokenToOP(TokenType tt) const {
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

inline std::string IREmitter::getScopeName(const ScopeNode *scope) const {
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

inline std::string IREmitter::mangle_struct(const CollectedItem &item) const {
  auto qualified = getScopeName(item.owner_scope);
  if (!qualified.empty())
    qualified += "_";
  qualified += item.name;
  return "_Struct_" + qualified;
}

inline std::string
IREmitter::mangle_constant(const std::string &name,
                           const ScopeNode *owner_scope,
                           const std::optional<std::string> &member_of) const {
  std::string qualified = getScopeName(owner_scope);
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

inline std::string
IREmitter::mangle_function(const FunctionMetaData &meta,
                           const ScopeNode *owner_scope,
                           const std::optional<std::string> &member_of) const {
  std::string qualified = getScopeName(owner_scope);
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

inline CollectedItem *
IREmitter::resolve_value_item(const std::string &name) const {
  for (auto *s = current_scope_node; s; s = s->parent) {
    if (auto *item = s->find_value_item(name)) {
      return item;
    }
  }
  return nullptr;
}

inline const CollectedItem *
IREmitter::resolve_struct_item(const std::string &name) const {
  for (auto *s = current_scope_node; s; s = s->parent) {
    if (auto *item = s->find_type_item(name)) {
      return item;
    }
  }
  return nullptr;
}

inline FuncPtr IREmitter::find_function(const FunctionMetaData *meta) const {
  auto it = function_table_.find(meta);
  if (it != function_table_.end()) {
    return it->second;
  }
  return nullptr;
}

inline ValuePtr IREmitter::function_symbol(const FunctionMetaData &meta,
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

inline ValuePtr IREmitter::resolve_ptr(ValuePtr value, const SemType &expected,
                                       const std::string &name) {
  auto expectedTy = context->resolveType(expected);
  auto valPtr = std::dynamic_pointer_cast<const PointerType>(value->type());
  auto expPtr = std::dynamic_pointer_cast<const PointerType>(expectedTy);

  if (expPtr) {
    if (valPtr) {
      auto current = value;
      auto currentPtrTy = valPtr;
      // Peel pointer
      while (currentPtrTy && !typeEquals(current->type(), expectedTy)) {
        auto innerPtrTy = std::dynamic_pointer_cast<const PointerType>(
            currentPtrTy->pointee());
        if (!innerPtrTy) {
          break;
        }
        // Don't load aggregates when peeling pointers
        if (isAggregateType(currentPtrTy->pointee())) {
          break;
        }
        current =
            current_block_->append<LoadInst>(current, currentPtrTy->pointee());
        currentPtrTy = innerPtrTy;
      }
      if (typeEquals(current->type(), expectedTy)) {
        LOG_DEBUG(
            "[IREmitter] resolve_ptr: matched pointer for expected reference");
        return current;
      }
      value = current;
      valPtr = std::dynamic_pointer_cast<const PointerType>(value->type());
    }
    auto tmp = createAlloca(expPtr->pointee(), name.empty() ? "tmp" : name);

    if (valPtr && isAggregateType(expPtr->pointee())) {
      std::size_t byteSize = computeTypeByteSize(expPtr->pointee());
      emitMemcpy(tmp, value, byteSize);
      LOG_DEBUG("[IREmitter] resolve_ptr: used memcpy for aggregate reference");
      return tmp;
    }

    ValuePtr toStore = value;
    if (valPtr) {
      toStore = current_block_->append<LoadInst>(value, valPtr->pointee());
    }
    if (!typeEquals(toStore->type(), expPtr->pointee())) {
      throw IRException("resolve_ptr: reference type mismatch");
    }
    current_block_->append<StoreInst>(toStore, tmp);
    LOG_DEBUG("[IREmitter] resolve_ptr: created temporary pointer for value");
    return tmp;
  }

  if (valPtr) {
    LOG_DEBUG("[IREmitter] resolve_ptr: loading from pointer");
    return current_block_->append<LoadInst>(value, expectedTy);
  }
  LOG_DEBUG("[IREmitter] resolve_ptr: value already a scalar");
  return value;
}

inline std::vector<SemType> IREmitter::build_effective_params(
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

inline SemType IREmitter::compute_self_type(const FunctionDecl &decl,
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

inline FuncPtr IREmitter::emit_function(const FunctionMetaData &meta,
                                        const FunctionDecl &node,
                                        const ScopeNode *scope,
                                        const std::vector<SemType> &params,
                                        const std::string &mangled_name) {
  (void)scope;
  LOG_DEBUG("[IREmitter] emit_function: " + meta.name +
            " mangled=" + mangled_name);
  std::vector<TypePtr> paramTyp;
  paramTyp.reserve(params.size());
  for (const auto &p : params) {
    paramTyp.push_back(context->resolveType(p));
  }

  auto originalRetTy = context->resolveType(meta.return_type);
  auto retTy = originalRetTy;
  bool useSret = isAggregateType(originalRetTy);

  if (useSret) {
    LOG_DEBUG(
        "[IREmitter] Using sret for function returning large aggregate: " +
        meta.name);
    auto sretPtrTy = std::make_shared<PointerType>(originalRetTy);
    paramTyp.insert(paramTyp.begin(), sretPtrTy);
    retTy = std::make_shared<VoidType>();
    sret_functions_[&meta] = originalRetTy;
  }

  auto fnTy = std::make_shared<FunctionType>(retTy, paramTyp, false);

  auto fn = create_function(mangled_name, fnTy, !node.body.has_value(), &meta);
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
  auto *previousScope = current_scope_node;
  current_block_ = fn->createBlock("entry");
  current_entry_block_ = current_block_;
  pushLocalScope();

  std::vector<std::string> paramNames;
  size_t argOffset = 0;

  // Handle sret parameter
  if (useSret) {
    fn->args()[0]->setName("sret");
    current_sret_ptr_ = fn->args()[0];
    argOffset = 1;
    LOG_DEBUG("[IREmitter] emit_function: bound sret parameter");
  } else {
    current_sret_ptr_ = nullptr;
  }

  if (node.self_param) {
    paramNames.emplace_back("self");
  }
  for (const auto &p : meta.param_names) {
    if (auto *id = dynamic_cast<IdentifierPattern *>(p.get())) {
      paramNames.push_back(id->name);
    }
  }

  for (size_t i = 0; i < paramNames.size(); ++i) {
    std::string paramName = paramNames[i];
    size_t argIdx = i + argOffset;
    fn->args()[argIdx]->setName(paramName);
    auto paramIrTy = paramTyp[argIdx];
    auto slot = createAlloca(paramIrTy, paramName);
    current_block_->append<StoreInst>(fn->args()[argIdx], slot);
    bindLocal(paramName, slot);
    LOG_DEBUG("[IREmitter] emit_function: bound param '" + paramName + "'");
  }

  if (node.body.has_value()) {
    if (auto *child = previousScope ? previousScope->find_child_scope_by_owner(
                                          node.body->get())
                                    : nullptr) {
      current_scope_node = child;
    }
    node.body.value()->accept(*this);
    ValuePtr result = operand_stack_.empty() ? nullptr : popOperand();
    bool terminated = current_block_->isTerminated();
    if (!terminated) {
      if (useSret) {
        // For sret, copy result to sret pointer and return void
        if (result) {
          auto resPtrTy =
              std::dynamic_pointer_cast<const PointerType>(result->type());
          if (resPtrTy && isAggregateType(originalRetTy)) {
            std::size_t byteSize = computeTypeByteSize(originalRetTy);
            emitMemcpy(current_sret_ptr_, result, byteSize);
          } else if (resPtrTy) {
            auto loaded =
                current_block_->append<LoadInst>(result, originalRetTy);
            current_block_->append<StoreInst>(loaded, current_sret_ptr_);
          } else {
            current_block_->append<StoreInst>(result, current_sret_ptr_);
          }
        }
        current_block_->append<ReturnInst>();
      } else if (fnTy->returnType()->kind() == TypeKind::Void) {
        current_block_->append<ReturnInst>();
      } else {
        if (!result) {
          result = std::make_shared<ConstantUnit>();
        }
        auto retIrTy = fnTy->returnType();
        auto resPtrTy =
            std::dynamic_pointer_cast<const PointerType>(result->type());

        result = loadPtrValue(result, meta.return_type);
        current_block_->append<ReturnInst>(result);
      }
    }
    current_scope_node = previousScope;
  }

  popLocalScope();
  current_block_ = saved_block;
  current_entry_block_ = saved_entry;
  current_sret_ptr_ = saved_sret_ptr;
  functions_.pop_back();
  LOG_INFO("[IREmitter] Finished emitting function: " + meta.name);
  return fn;
}

inline FuncPtr IREmitter::create_function(const std::string &name,
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
  auto fn = module_.createFunction(name, ty, is_external);
  function_table_[meta] = fn;
  return fn;
}

inline std::shared_ptr<ConstantString>
IREmitter::internStringLiteral(const std::string &data_with_null) {
  auto it = interned_strings_.find(data_with_null);
  if (it != interned_strings_.end()) {
    return it->second;
  }

  auto name = "_StrLit_" + std::to_string(next_string_id_++);
  auto cst = std::make_shared<ConstantString>(data_with_null);
  cst->setName(name);
  module_.createConstant(cst);
  interned_strings_[data_with_null] = cst;
  return cst;
}

inline char IREmitter::decodeCharLiteral(const std::string &literal) {
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
    auto hexVal = [](char ch) -> int {
      if (ch >= '0' && ch <= '9')
        return ch - '0';
      if (ch >= 'a' && ch <= 'f')
        return 10 + (ch - 'a');
      if (ch >= 'A' && ch <= 'F')
        return 10 + (ch - 'A');
      return -1;
    };
    int hi = hexVal(literal[3]);
    int lo = hexVal(literal[4]);
    if (hi < 0 || lo < 0) {
      throw IRException("invalid hex escape in char literal");
    }
    return ensure_ascii(static_cast<unsigned>((hi << 4) | lo));
  }
  default:
    return ensure_ascii(static_cast<unsigned char>(esc));
  }
}

inline std::string IREmitter::decodeStringLiteral(const std::string &literal) {
  if (literal.empty()) {
    return {};
  }

  std::size_t pos = 0;
  if (literal[pos] == 'c') {
    ++pos; // c"..." treated the same as normal
  }

  bool isRaw = false;
  std::size_t hashCount = 0;
  if (pos < literal.size() && literal[pos] == 'r') {
    isRaw = true;
    ++pos;
    while (pos < literal.size() && literal[pos] == '#') {
      ++hashCount;
      ++pos;
    }
  }

  auto firstQuote = literal.find('"', pos);
  auto lastQuote = literal.rfind('"');
  if (firstQuote == std::string::npos || lastQuote == std::string::npos ||
      lastQuote <= firstQuote) {
    // Already normalized or malformed; return as-is to avoid blocking IR
    return literal;
  }

  (void)hashCount; // hash count is validated by lexer/parser; silences unused
                   // warning

  // For raw strings, just slice out the payload between the matching quotes
  if (isRaw) {
    return literal.substr(firstQuote + 1, lastQuote - firstQuote - 1);
  }

  std::string body = literal.substr(firstQuote + 1, lastQuote - firstQuote - 1);
  std::string out;
  out.reserve(body.size());

  auto hexVal = [](char ch) -> int {
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
      int hi = hexVal(body[i + 1]);
      int lo = hexVal(body[i + 2]);
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

inline FuncPtr IREmitter::createMemsetFn() {
  // Signature: ptr @_Function_prelude_builtin_memset(ptr, i32, i32)
  auto ptrTy = std::make_shared<PointerType>(IntegerType::i32(false));
  auto i32Ty = IntegerType::i32(true);
  auto fnTy = std::make_shared<FunctionType>(
      ptrTy, std::vector<TypePtr>{ptrTy, i32Ty, i32Ty}, false);
  memset_fn_ =
      module_.createFunction("_Function_prelude_builtin_memset", fnTy, true);
  LOG_DEBUG("[IREmitter] Created builtin_memset function declaration");
  return memset_fn_;
}

inline FuncPtr IREmitter::createMemcpyFn() {
  // Signature: ptr @_Function_prelude_builtin_memcpy(ptr, ptr, i32)
  auto ptrTy =
      std::make_shared<PointerType>(std::make_shared<IntegerType>(32, false));
  auto i32Ty = IntegerType::i32(true);
  auto fnTy = std::make_shared<FunctionType>(
      ptrTy, std::vector<TypePtr>{ptrTy, ptrTy, i32Ty}, false);
  memcpy_fn_ =
      module_.createFunction("_Function_prelude_builtin_memcpy", fnTy, true);
  LOG_DEBUG("[IREmitter] Created builtin_memcpy function declaration");
  return memcpy_fn_;
}

inline std::size_t alignTo(std::size_t value, std::size_t align) {
  if (align <= 1) {
    return value;
  }
  auto rem = value % align;
  return rem ? (value + (align - rem)) : value;
}

inline IREmitter::TypeLayoutInfo
IREmitter::computeTypeLayout(const TypePtr &ty) const {
  if (auto intTy = std::dynamic_pointer_cast<const IntegerType>(ty)) {
    std::size_t size = (intTy->bits() + 7) / 8;
    if (size == 0) {
      size = 1;
    }
    return {size, size};
  }
  if (auto arrTy = std::dynamic_pointer_cast<const ArrayType>(ty)) {
    auto elem = computeTypeLayout(arrTy->elem());
    std::size_t stride = alignTo(elem.size, elem.align);
    return {stride * arrTy->count(), elem.align};
  }
  if (auto structTy = std::dynamic_pointer_cast<const StructType>(ty)) {
    std::size_t offset = 0;
    std::size_t maxAlign = 1;
    for (const auto &field : structTy->fields()) {
      auto layout = computeTypeLayout(field);
      offset = alignTo(offset, layout.align);
      offset += layout.size;
      maxAlign = std::max(maxAlign, layout.align);
    }
    offset = alignTo(offset, maxAlign);
    return {offset, maxAlign};
  }
  if (std::dynamic_pointer_cast<const PointerType>(ty)) {
    std::size_t ptrBytes = module_.target().pointerWidth
                               ? module_.target().pointerWidth / 8
                               : sizeof(void *);
    return {ptrBytes, ptrBytes};
  }
  if (ty->isVoid()) {
    return {0, 1};
  }
  throw IRException("unknown type");
}

inline std::size_t IREmitter::computeTypeByteSize(const TypePtr &ty) const {
  return computeTypeLayout(ty).size;
}

inline bool IREmitter::isZeroLiteral(const Expression *expr) const {
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

inline bool IREmitter::isAggregateType(const TypePtr &ty) const {
  return ty->kind() == TypeKind::Array || ty->kind() == TypeKind::Struct;
}

inline void IREmitter::emitMemcpy(ValuePtr dst, ValuePtr src,
                                  std::size_t byteSize) {
  if (byteSize == 0) {
    return;
  }
  auto memcpySymbol =
      std::make_shared<Value>(memcpy_fn_->type(), memcpy_fn_->name());
  std::vector<ValuePtr> args;
  args.push_back(dst);
  args.push_back(src);
  args.push_back(
      ConstantInt::getI32(static_cast<std::uint32_t>(byteSize), true));
  auto ptrTy = std::make_shared<PointerType>(IntegerType::i32(false));
  current_block_->append<CallInst>(memcpySymbol, args, ptrTy);
}

} // namespace rc::ir
