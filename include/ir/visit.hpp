#pragma once

#include <algorithm>
#include <cassert>
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
  std::vector<std::shared_ptr<Function>> functions_;
  std::vector<std::shared_ptr<StructType>> struct_types_;
  std::shared_ptr<BasicBlock> current_block_;

  std::unordered_map<const BaseNode *, std::string> name_mangle_;
  std::unordered_map<const FunctionMetaData *, std::shared_ptr<Function>>
      function_table_;
  std::unordered_map<const FunctionMetaData *, std::shared_ptr<Value>>
      function_symbols_;
  std::unordered_map<std::string, std::shared_ptr<Value>> globals_;

  std::vector<std::unordered_map<std::string, std::shared_ptr<Value>>>
      locals_; // local mapped to their memory location or SSA

  std::vector<std::shared_ptr<Value>> operand_stack_;
  std::vector<std::shared_ptr<BasicBlock>> block_stack_; // for break/continue

  std::shared_ptr<Value> popOperand();
  void pushOperand(std::shared_ptr<Value> v);
  std::shared_ptr<Value> loadPtrValue(std::shared_ptr<Value> v,
                                      const SemType &semTy);
  std::shared_ptr<Value> lookupLocal(const std::string &name) const;
  void bindLocal(const std::string &name, std::shared_ptr<Value> v);
  void pushLocalScope();
  void popLocalScope();

  bool is_assignment_token(TokenType tt) const;
  bool is_integer(SemPrimitiveKind k) const;

  std::optional<BinaryOpKind> token_to_binop(TokenType tt) const;

  // mangling helpers
  std::string qualify_scope(const ScopeNode *scope) const;
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
  std::shared_ptr<Function> find_function(const FunctionMetaData *meta) const;
  std::shared_ptr<Function> create_function(const std::string &name,
                                            std::shared_ptr<FunctionType> ty,
                                            bool is_external,
                                            const FunctionMetaData *meta);
  std::shared_ptr<Value> function_symbol(const FunctionMetaData &meta,
                                         const std::shared_ptr<Function> &fn);

  // argument utilities
  std::shared_ptr<Value> resolve_ptr(std::shared_ptr<Value> value,
                                     const SemType &expected,
                                     const std::string &name_hint);
  std::vector<SemType> build_effective_params(
      const FunctionMetaData &meta,
      const std::optional<SemType> &self_type = std::nullopt) const;
  SemType compute_self_type(const FunctionDecl &decl,
                            const CollectedItem *owner) const;

  std::shared_ptr<Function> emit_function(const FunctionMetaData &meta,
                                          const FunctionDecl &node,
                                          const ScopeNode *scope,
                                          const std::vector<SemType> &params,
                                          const std::string &mangled_name);
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
  block_stack_.clear();
  name_mangle_.clear();
  function_table_.clear();
  function_symbols_.clear();
  globals_.clear();
  functions_.clear();
  struct_types_.clear();
  locals_.emplace_back();
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
  } else {
    LOG_DEBUG(std::string("[IREmitter] Unhandled node type: ") +
              typeid(node).name());
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
  LOG_DEBUG("[IREmitter] Visiting BinaryExpression op=" + node.op.lexeme);
  if (is_assignment_token(node.op.type)) {
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
      current_block_->append<StoreInst>(rhs, lhs);
      pushOperand(rhs);
      return;
    }

    auto loaded = current_block_->append<LoadInst>(lhs, lhsPtrTy->pointee());
    auto opKind = token_to_binop(node.op.type);
    if (!opKind) {
      LOG_ERROR("[IREmitter] unsupported compound assignment operator");
      throw IRException("unsupported compound assignment operator");
    }
    auto combined = current_block_->append<BinaryOpInst>(*opKind, loaded, rhs,
                                                         lhsPtrTy->pointee());
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

    LOG_DEBUG("[IREmitter] Falling back to emitting value for cast");
    pushOperand(val);
    return;
  }

  node.left->accept(*this);
  auto lhs = popOperand();
  node.right->accept(*this);
  auto rhs = popOperand();
  lhs = loadPtrValue(lhs, context->lookupType(node.left.get()));
  rhs = loadPtrValue(rhs, context->lookupType(node.right.get()));

  auto opKind = token_to_binop(node.op.type);
  if (opKind) {
    LOG_DEBUG("[IREmitter] Emitting binary arithmetic/logical op");
    auto result =
        current_block_->append<BinaryOpInst>(*opKind, lhs, rhs, lhs->type());
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
    LOG_ERROR("[IREmitter] unsupported binary operator: " +
              std::to_string(static_cast<int>(node.op.type)));
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
    v = loadPtrValue(v, context->lookupType(node.value->get()));
    current_block_->append<ReturnInst>(v);
  } else {
    current_block_->append<ReturnInst>();
  }
}

inline void IREmitter::visit(PrefixExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting prefix operator " + node.op.lexeme);
  // visit, modify, store
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block for prefix expression");
    throw IRException("no active basic block");
  }
  node.right->accept(*this);
  auto operand = popOperand();
  const auto semTy = context->lookupType(&node);
  operand = loadPtrValue(operand, semTy);
  auto irTy = context->resolveType(semTy);

  switch (node.op.type) {
  case TokenType::NOT: {
    auto lhs = std::make_shared<ConstantInt>(IntegerType::i1(), 1);
    pushOperand(current_block_->append<BinaryOpInst>(BinaryOpKind::XOR, lhs,
                                                     operand, irTy));
    break;
  }
  case TokenType::MINUS: {
    auto intTy = std::dynamic_pointer_cast<const IntegerType>(irTy);
    if (!intTy) {
      LOG_ERROR("[IREmitter] unary minus on non-integer");
      throw IRException("unary minus on non-integer");
    }
    auto zeroTy =
        std::make_shared<IntegerType>(intTy->bits(), intTy->isSigned());
    auto zero = std::make_shared<ConstantInt>(zeroTy, 0);
    pushOperand(current_block_->append<BinaryOpInst>(BinaryOpKind::SUB, zero,
                                                     operand, zeroTy));
    break;
  }
  default:
    LOG_ERROR("[IREmitter] unsupported prefix operator");
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

  auto slot = current_block_->append<AllocaInst>(irTy, nullptr, 0, ident->name);
  current_block_->append<StoreInst>(init, slot);
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

  if (semTy.is_primitive()) {
    switch (semTy.as_primitive().kind) {
    case SemPrimitiveKind::BOOL: {
      bool value = node.value == "true";
      pushOperand(ConstantInt::getI1(value));
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
    LOG_ERROR("[IREmitter] unsupported literal type for literal: " +
              node.value);
    // TODO: support char & string literals
    throw IRException("unsupported literal type");
  }
  std::string cleaned;
  cleaned.reserve(node.value.size());
  for (char c : node.value) {
    if (c != '_')
      cleaned.push_back(c);
  }
  std::uint64_t parsed = 0;
  parsed = static_cast<std::uint64_t>(std::stoll(cleaned, nullptr, 0));
  pushOperand(std::make_shared<ConstantInt>(
      std::make_shared<IntegerType>(intTy->bits(), intTy->isSigned()), parsed));
}

inline void IREmitter::visit(NameExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting name expression: " + node.name);
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block");
    throw IRException("no active basic block");
  }
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
  if (!value) {
    LOG_ERROR("[IREmitter] unknown identifier " + node.name);
    throw IRException("unknown identifier " + node.name);
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
  } else if (cv.type.is_primitive() &&
             cv.type.as_primitive().kind == SemPrimitiveKind::UNIT) {
    irConst = std::make_shared<ConstantUnit>();
  } else {
    LOG_ERROR("[IREmitter] constant '" + node.name +
              "' has unsupported type for IR emission");
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
  std::vector<std::shared_ptr<Value>> args;
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
    if (pathExpr->leading_colons) {
      LOG_ERROR("[IREmitter] leading colons in paths are not supported");
      throw IRException("leading colons in paths are not supported");
    }
    if (pathExpr->segments.size() != 2) {
      LOG_ERROR("[IREmitter] only TypeName::function calls are supported");
      throw IRException("only TypeName::function calls are supported");
    }
    for (const auto &seg : pathExpr->segments) {
      if (seg.call.has_value()) {
        LOG_ERROR("[IREmitter] path segment expressions are not supported");
        throw IRException("path segment expressions are not supported");
      }
    }

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
    if (!meta) {
      LOG_ERROR("[IREmitter] unknown method '" + fn_name + "' on type '" +
                type_name + "'");
      throw IRException("unknown method '" + fn_name + "' on type '" +
                        type_name + "'");
    }
    if (meta->decl && meta->decl->self_param.has_value()) {
      LOG_ERROR(
          "[IREmitter] cannot call instance method as associated function");
      throw IRException("cannot call instance method '" + fn_name +
                        "' as associated function");
    }
    owner_scope = type_item->owner_scope;
    member_of = type_item->name;
  } else {
    LOG_ERROR("[IREmitter] unsupported call target");
    throw IRException("unsupported call target");
  }

  auto paramSems = build_effective_params(*meta); // handle &self

  std::vector<TypePtr> irParams;
  irParams.reserve(paramSems.size());
  for (const auto &p : paramSems) {
    irParams.push_back(context->resolveType(p));
  }
  auto retTy = context->resolveType(meta->return_type);

  auto found = find_function(meta);
  if (!found) { // this happens for builtins or forward declarations
    LOG_DEBUG("[IREmitter] Predeclaring function '" + meta->name + "'");
    auto fnTy = std::make_shared<FunctionType>(retTy, irParams, false);
    auto mangled = mangle_function(*meta, owner_scope, member_of);
    found = create_function(mangled, fnTy,
                            !meta->decl || !meta->decl->body.has_value(), meta);
  }

  auto callee = function_symbol(*meta, found);

  std::vector<std::shared_ptr<Value>> resolved;
  resolved.reserve(args.size());
  for (size_t i = 0; i < args.size(); ++i) {
    resolved.push_back(
        resolve_ptr(args[i], paramSems[i], "arg" + std::to_string(i)));
  }

  LOG_DEBUG("[IREmitter] Emitting call to function " + meta->name);
  auto callInst = current_block_->append<CallInst>(callee, resolved, retTy);
  pushOperand(callInst);
}

inline void IREmitter::visit(StructDecl &node) {
  LOG_DEBUG("[IREmitter] Visiting struct declaration: " + node.name);
  auto *item = current_scope_node
                   ? current_scope_node->find_type_item(node.name)
                   : nullptr;
  if (!item || !item->has_struct_meta()) {
    LOG_ERROR("[IREmitter] struct metadata not found for '" + node.name + "'");
    throw IRException("struct metadata not found for '" + node.name + "'");
  }
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
    } else if (cv.type.is_primitive() &&
               cv.type.as_primitive().kind == SemPrimitiveKind::UNIT) {
      irConst = std::make_shared<ConstantUnit>();
    } else {
      // TODO: support char & string constants
      LOG_DEBUG("[IREmitter] Skipping non-primitive associated constant: " +
                c.name);
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
    auto retTy = context->resolveType(m.return_type);
    auto fnTy = std::make_shared<FunctionType>(retTy, irParams, false);
    auto mangled_fn = mangle_function(m, item->owner_scope, item->name);
    create_function(mangled_fn, fnTy, !m.decl || !m.decl->body.has_value(), &m);
    LOG_DEBUG("[IREmitter] Precreated method function: " + m.name +
              " mangled=" + mangled_fn);
  }

  return;
}

inline void IREmitter::visit(EnumDecl &) {
  LOG_ERROR("[IREmitter] EnumDecl emission not implemented");
  throw std::runtime_error("EnumDecl emission not implemented");
}

inline void IREmitter::visit(TraitDecl &) {
  LOG_ERROR("[IREmitter] TraitDecl emission not implemented");
  throw std::runtime_error("TraitDecl emission not implemented");
}

inline void IREmitter::visit(ImplDecl &node) {
  LOG_DEBUG("[IREmitter] Visiting impl declaration");
  if (node.impl_type != ImplDecl::ImplType::Inherent) {
    LOG_ERROR("[IREmitter] trait impl removed");
    throw IRException("trait impl removed");
  }

  if (!node.target_type.is_path()) {
    LOG_ERROR("[IREmitter] unsupported impl target type");
    throw IRException("unsupported impl target type");
  }

  const auto &segments = node.target_type.as_path().segments;
  if (segments.size() != 1) {
    LOG_ERROR("[IREmitter] qualified impl targets are not supported");
    throw IRException("qualified impl targets are not supported");
  }
  const std::string &target_name = segments[0];

  const auto *struct_item = resolve_struct_item(target_name);

  const auto &meta = struct_item->as_struct_meta();
  std::unordered_set<const FunctionDecl *> visited;

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
      if (!found) {
        LOG_ERROR("[IREmitter] method metadata missing for '" + fn->name + "'");
        throw IRException("method metadata missing for '" + fn->name + "'");
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

  if (node.else_block) {
    auto elseBlock = cur_func->createBlock("if_else");
    auto mergeBlock = cur_func->createBlock("if_merge");
    current_block_->append<BranchInst>(condVal, thenBlock, elseBlock);
    // then block
    current_block_ = thenBlock;
    if (node.then_block)
      node.then_block->accept(*this);
    auto thenVal = popOperand();
    current_block_->append<BranchInst>(mergeBlock);
    // else block
    current_block_ = elseBlock;
    if (node.else_block)
      std::static_pointer_cast<BlockExpression>(node.else_block.value())
          ->accept(*this);
    auto elseVal = popOperand();
    current_block_->append<BranchInst>(mergeBlock);
    // merge block
    current_block_ = mergeBlock;
  } else {
    auto mergeBlock = cur_func->createBlock("if_merge");
    current_block_->append<BranchInst>(condVal, thenBlock, mergeBlock);
    // then block
    current_block_ = thenBlock;
    if (node.then_block)
      node.then_block->accept(*this);
    auto thenVal = popOperand();
    current_block_->append<BranchInst>(mergeBlock);
    // merge block
    current_block_ = mergeBlock;
  }
}

inline void IREmitter::visit(MethodCallExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting method call: " + node.method_name.name);
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block for method call");
    throw IRException("no active basic block for method call");
  }

  node.receiver->accept(*this);
  auto receiverVal = popOperand();
  auto recvSem = context->lookupType(node.receiver.get());
  SemType lookupType = recvSem;
  if (lookupType.is_reference()) {
    lookupType = *lookupType.as_reference().target;
  }

  if (!lookupType.is_named()) {
    LOG_ERROR("[IREmitter] method call on non-struct type");
    throw IRException("method call on non-struct type");
  }
  const CollectedItem *ci = lookupType.as_named().item;
  if (!ci || !ci->has_struct_meta()) {
    LOG_ERROR("[IREmitter] method call target is not a struct");
    throw IRException("method call target is not a struct");
  }

  const auto &meta = ci->as_struct_meta();
  const FunctionMetaData *found = nullptr;
  for (const auto &m : meta.methods) {
    if (m.name == node.method_name.name) {
      found = &m;
      break;
    }
  }
  if (!found) {
    LOG_ERROR("[IREmitter] unknown method '" + node.method_name.name + "'");
    throw IRException("unknown method '" + node.method_name.name + "'");
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
  auto retTy = context->resolveType(found->return_type);

  auto foundFn = find_function(found);
  if (!foundFn) {
    auto fnTy = std::make_shared<FunctionType>(retTy, paramIr, false);
    auto mangled = mangle_function(*found, ci->owner_scope, ci->name);
    foundFn = create_function(
        mangled, fnTy, !found->decl || !found->decl->body.has_value(), found);
    LOG_DEBUG("[IREmitter] Predeclared method function: " + found->name +
              " mangled=" + mangled);
  }
  auto callee = function_symbol(*found, foundFn);

  std::vector<std::shared_ptr<Value>> args;
  args.reserve(paramSems.size());
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
  pushOperand(call);
  LOG_DEBUG("[IREmitter] Emitted method call to " + found->name);
}

inline void IREmitter::visit(FieldAccessExpression &node) {

  // TODO: support multiple struct levels

  LOG_DEBUG("[IREmitter] Visiting field access: " + node.field_name);
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block for field access");
    throw IRException("no active basic block for field access");
  }
  node.target->accept(*this);
  auto targetVal = popOperand();

  auto targetType = context->lookupType(node.target.get());
  if (targetType.is_reference()) {
    targetType = *targetType.as_reference().target;
  }

  if (!targetType.is_named()) {
    LOG_ERROR("[IREmitter] field access on non-struct type");
    throw IRException("field access on non-struct type");
  }
  const CollectedItem *ci = targetType.as_named().item;
  if (!ci || !ci->has_struct_meta()) {
    LOG_ERROR("[IREmitter] field access target is not a struct");
    throw IRException("field access target is not a struct");
  }
  const auto &meta = ci->as_struct_meta();
  size_t index = 0;
  bool found = false;
  SemType fieldSem;
  for (size_t i = 0; i < meta.named_fields.size(); ++i) {
    if (meta.named_fields[i].first == node.field_name) {
      index = i;
      fieldSem = meta.named_fields[i].second;
      found = true;
      break;
    }
  }
  if (!found) {
    LOG_ERROR("[IREmitter] unknown field '" + node.field_name + "'");
    throw IRException("unknown field '" + node.field_name + "'");
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

  auto zero = ConstantInt::getI32(0, false);
  auto idxConst = ConstantInt::getI32(static_cast<std::uint32_t>(index), false);
  auto fieldIrTy = context->resolveType(fieldSem);
  auto gep = current_block_->append<GetElementPtrInst>(
      fieldIrTy, structPtr, std::vector<std::shared_ptr<Value>>{zero, idxConst},
      node.field_name);

  auto loaded = current_block_->append<LoadInst>(gep, fieldIrTy, 0, false,
                                                 node.field_name);
  pushOperand(loaded);
  LOG_DEBUG("[IREmitter] Loaded field '" + node.field_name + "'");
}

inline void IREmitter::visit(StructExpression &node) {
  LOG_DEBUG("[IREmitter] Emitting struct expression");
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block for struct expression");
    throw IRException("no active basic block for struct expression");
  }

  auto *nameExpr = dynamic_cast<NameExpression *>(node.path_expr.get());
  if (!nameExpr) {
    LOG_ERROR("[IREmitter] struct expression requires identifier path");
    throw IRException("struct expression requires identifier path");
  }

  const auto *item = resolve_struct_item(nameExpr->name);
  if (!item || !item->has_struct_meta()) {
    LOG_ERROR("[IREmitter] unknown struct '" + nameExpr->name + "'");
    throw IRException("unknown struct '" + nameExpr->name + "'");
  }

  const auto &meta = item->as_struct_meta();
  if (meta.named_fields.size() != node.fields.size()) {
    LOG_ERROR("[IREmitter] struct expression field count mismatch for " +
              nameExpr->name);
    throw IRException("struct expression field count mismatch");
  }

  std::unordered_map<std::string, std::shared_ptr<Expression>> provided;
  for (const auto &f : node.fields) {
    if (!f.value) {
      LOG_ERROR("[IREmitter] missing value for struct field in expression");
      throw IRException("missing value for field '" + f.name + "'");
    }
    provided[f.name] = f.value.value();
  }

  auto structSem = SemType::named(item);
  auto structIrTy = context->resolveType(structSem);
  auto slot =
      current_block_->append<AllocaInst>(structIrTy, nullptr, 0, "structtmp");

  auto zero = ConstantInt::getI32(0, false);
  for (size_t i = 0; i < meta.named_fields.size(); ++i) {
    const auto &field = meta.named_fields[i];
    auto it = provided.find(field.first);
    if (it == provided.end()) {
      LOG_ERROR("[IREmitter] missing initializer for field '" + field.first +
                "'");
      throw IRException("missing initializer for field '" + field.first + "'");
    }
    it->second->accept(*this);
    auto val = popOperand();
    auto resolved = resolve_ptr(val, field.second, field.first);
    auto idxConst = ConstantInt::getI32(static_cast<std::uint32_t>(i), false);
    auto fieldTy = context->resolveType(field.second);
    auto gep = current_block_->append<GetElementPtrInst>(
        fieldTy, slot, std::vector<std::shared_ptr<Value>>{zero, idxConst},
        field.first);
    current_block_->append<StoreInst>(resolved, gep);
  }

  auto loaded = current_block_->append<LoadInst>(slot, structIrTy);
  pushOperand(loaded);
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
  block_stack_.push_back(afterBlock); // break in loop goes to afterBlock
  node.body->accept(*this);
  current_block_->append<BranchInst>(bodyBlock);
  block_stack_.pop_back();

  current_block_ = afterBlock;
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
  block_stack_.push_back(afterBlock); // break in while goes to afterBlock
  node.body->accept(*this);
  current_block_->append<BranchInst>(condBlock);
  block_stack_.pop_back();

  current_block_ = afterBlock;
  LOG_DEBUG("[IREmitter] Finished while expression");
}

inline void IREmitter::visit(ArrayExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting array expression");
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block for array expression");
    throw IRException("no active basic block for array expression");
  }

  auto semTy = context->lookupType(&node);
  if (!semTy.is_array()) {
    LOG_ERROR("[IREmitter] array expression type not resolved to array");
    throw IRException("array expression type not resolved to array");
  }
  const auto &arrSem = semTy.as_array();
  std::size_t count = static_cast<std::size_t>(arrSem.size);
  if (node.actual_size >= 0) {
    count = static_cast<std::size_t>(node.actual_size);
  }

  auto arrIrTy = context->resolveType(semTy);
  auto elemIrTy = context->resolveType(*arrSem.element);
  auto slot = current_block_->append<AllocaInst>(arrIrTy, nullptr, 0, "arrtmp");
  auto zero = ConstantInt::getI32(0, false);

  if (node.repeat) {
    node.repeat->first->accept(*this);
    auto val = popOperand();
    auto resolved = resolve_ptr(val, *arrSem.element, "arr_init");
    std::size_t repeatCount = count;
    for (std::size_t i = 0; i < repeatCount; ++i) {
      auto idxConst = ConstantInt::getI32(static_cast<std::uint32_t>(i), false);
      auto gep = current_block_->append<GetElementPtrInst>(
          elemIrTy, slot, std::vector<std::shared_ptr<Value>>{zero, idxConst},
          "elt");
      current_block_->append<StoreInst>(resolved, gep);
    }
  } else {
    if (node.elements.size() != count) {
      LOG_ERROR("[IREmitter] array literal size mismatch");
      throw IRException("array literal size mismatch");
    }
    for (std::size_t i = 0; i < node.elements.size(); ++i) {
      node.elements[i]->accept(*this);
      auto val = popOperand();
      auto resolved =
          resolve_ptr(val, *arrSem.element, "elt" + std::to_string(i));
      auto idxConst = ConstantInt::getI32(static_cast<std::uint32_t>(i), false);
      auto gep = current_block_->append<GetElementPtrInst>(
          elemIrTy, slot, std::vector<std::shared_ptr<Value>>{zero, idxConst},
          "elt");
      current_block_->append<StoreInst>(resolved, gep);
    }
  }

  auto loaded = current_block_->append<LoadInst>(slot, arrIrTy);
  pushOperand(loaded);
  LOG_DEBUG("[IREmitter] Constructed array of count " + std::to_string(count));
}

inline void IREmitter::visit(IndexExpression &node) {
  LOG_DEBUG("[IREmitter] Visiting index expression");
  if (!current_block_) {
    LOG_ERROR("[IREmitter] no active basic block for index expression");
    throw IRException("no active basic block for index expression");
  }

  node.target->accept(*this);
  auto targetVal = popOperand();
  auto targetSem = context->lookupType(node.target.get());
  if (targetSem.is_reference()) {
    targetSem = *targetSem.as_reference().target;
  }

  const auto &arrSem = targetSem.as_array();
  auto elemIrTy = context->resolveType(*arrSem.element);
  auto basePtr = std::dynamic_pointer_cast<const PointerType>(targetVal->type())
                     ? targetVal
                     : nullptr;
  if (!basePtr) {
    auto arrIrTy = context->resolveType(targetSem);
    auto tmp =
        current_block_->append<AllocaInst>(arrIrTy, nullptr, 0, "indextmp");
    current_block_->append<StoreInst>(targetVal, tmp);
    basePtr = tmp;
  }

  node.index->accept(*this);
  auto idxVal = popOperand();
  auto idxTy = std::dynamic_pointer_cast<const IntegerType>(idxVal->type());
  if (!idxTy) {
    LOG_ERROR("[IREmitter] array index must be integer");
    throw IRException("array index must be integer");
  }

  auto zero = ConstantInt::getI32(0, false);
  auto gep = current_block_->append<GetElementPtrInst>(
      elemIrTy, basePtr, std::vector<std::shared_ptr<Value>>{zero, idxVal},
      "idx");
  auto loaded = current_block_->append<LoadInst>(gep, elemIrTy);
  pushOperand(loaded);
  LOG_DEBUG("[IREmitter] Emitted index access");
}

inline void IREmitter::visit(TupleExpression &) {
  LOG_ERROR("[IREmitter] TupleExpression emission not implemented");
  throw std::runtime_error("TupleExpression emission not implemented");
}

inline void IREmitter::visit(BreakExpression &) {
  LOG_DEBUG("[IREmitter] Visiting break expression");
  if (block_stack_.empty()) {
    LOG_ERROR("[IREmitter] break used outside of loop");
    throw IRException("break used outside of loop"); // this should never happen
  }
  auto targetBlock = block_stack_.back();
  current_block_->append<BranchInst>(targetBlock);
  // should never have dead code, but just in case
  auto cur_func = functions_.back();
  auto unreachableBlock = cur_func->createBlock("unreachable_after_break");
  current_block_ = unreachableBlock;
}

inline void IREmitter::visit(ContinueExpression &) {
  LOG_DEBUG("[IREmitter] Visiting continue expression");
  if (block_stack_.empty()) {
    LOG_ERROR("[IREmitter] continue used outside of loop");
    throw IRException(
        "continue used outside of loop"); // this should never happen
  }
  auto targetBlock = block_stack_.back();
  current_block_->append<BranchInst>(targetBlock);
  // should never have dead code, but just in case
  auto cur_func = functions_.back();
  auto unreachableBlock = cur_func->createBlock("unreachable_after_continue");
  current_block_ = unreachableBlock;
}

inline void IREmitter::visit(PathExpression &) {
  LOG_ERROR("[IREmitter] PathExpression emission not implemented");
  throw std::runtime_error("PathExpression emission not implemented");
}

inline void IREmitter::visit(QualifiedPathExpression &) {
  LOG_ERROR("[IREmitter] QualifiedPathExpression emission not implemented");
  throw std::runtime_error("QualifiedPathExpression emission not implemented");
}

inline void IREmitter::visit(BorrowExpression &) {
  LOG_ERROR("[IREmitter] BorrowExpression emission not implemented");
  throw std::runtime_error("BorrowExpression emission not implemented");
}

inline void IREmitter::visit(DerefExpression &) {
  LOG_ERROR("[IREmitter] DerefExpression emission not implemented");
  throw std::runtime_error("DerefExpression emission not implemented");
}

inline std::shared_ptr<Value> IREmitter::popOperand() {
  if (operand_stack_.empty()) {
    LOG_ERROR("[IREmitter] operand stack underflow");
    throw IRException("operand stack underflow");
  }
  auto v = operand_stack_.back();
  operand_stack_.pop_back();
  LOG_DEBUG("[IREmitter] popOperand -> " + (v ? v->name() : "<null>"));
  return v;
}

inline void IREmitter::pushOperand(std::shared_ptr<Value> v) {
  if (!v) {
    LOG_ERROR("[IREmitter] attempt to push null operand");
    throw IRException("attempt to push null operand");
  }
  LOG_DEBUG("[IREmitter] pushOperand -> " + v->name());
  operand_stack_.push_back(std::move(v));
}

inline std::shared_ptr<Value> IREmitter::loadPtrValue(std::shared_ptr<Value> v,
                                                      const SemType &semTy) {
  if (!v) {
    LOG_ERROR("[IREmitter] attempt to materialize null value");
    throw IRException("attempt to materialize null value");
  }
  auto ptrTy = std::dynamic_pointer_cast<const PointerType>(v->type());
  if (ptrTy && !semTy.is_reference()) {
    LOG_DEBUG("[IREmitter] loadPtrValue loading from pointer");
    return current_block_->append<LoadInst>(v, ptrTy->pointee());
  }
  return v;
}

inline std::shared_ptr<Value>
IREmitter::lookupLocal(const std::string &name) const {
  for (auto it = locals_.rbegin(); it != locals_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return nullptr;
}

inline void IREmitter::bindLocal(const std::string &name,
                                 std::shared_ptr<Value> v) {
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

inline bool IREmitter::is_assignment_token(TokenType tt) const {
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

inline bool IREmitter::is_integer(SemPrimitiveKind k) const {
  return k == SemPrimitiveKind::ANY_INT || k == SemPrimitiveKind::I32 ||
         k == SemPrimitiveKind::U32 || k == SemPrimitiveKind::ISIZE ||
         k == SemPrimitiveKind::USIZE;
}

inline std::optional<BinaryOpKind>
IREmitter::token_to_binop(TokenType tt) const {
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

inline std::string IREmitter::qualify_scope(const ScopeNode *scope) const {
  std::vector<std::string> parts;
  for (auto *s = scope; s; s = s->parent) {
    if (!s->name.empty())
      parts.push_back(s->name);
  }
  std::reverse(parts.begin(), parts.end());
  std::string qualified;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i)
      qualified += "::";
    qualified += parts[i];
  }
  return qualified;
}

inline std::string IREmitter::mangle_struct(const CollectedItem &item) const {
  auto qualified = qualify_scope(item.owner_scope);
  if (!qualified.empty())
    qualified += "::";
  qualified += item.name;
  return "_RS" + qualified;
}

inline std::string
IREmitter::mangle_constant(const std::string &name,
                           const ScopeNode *owner_scope,
                           const std::optional<std::string> &member_of) const {
  std::string qualified = qualify_scope(owner_scope);
  if (member_of) {
    if (!qualified.empty())
      qualified += "::";
    qualified += *member_of;
  }
  if (!qualified.empty())
    qualified += "::";
  qualified += name;
  return "_RC" + qualified;
}

inline std::string
IREmitter::mangle_function(const FunctionMetaData &meta,
                           const ScopeNode *owner_scope,
                           const std::optional<std::string> &member_of) const {
  std::string qualified = qualify_scope(owner_scope);
  if (member_of) {
    if (!qualified.empty())
      qualified += "::";
    qualified += *member_of;
  }
  if (!qualified.empty())
    qualified += "::";
  qualified += meta.name;

  std::string signature = "_";
  return "_RF" + qualified + signature;
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

inline std::shared_ptr<Function>
IREmitter::find_function(const FunctionMetaData *meta) const {
  auto it = function_table_.find(meta);
  if (it != function_table_.end()) {
    return it->second;
  }
  return nullptr;
}

inline std::shared_ptr<Value>
IREmitter::function_symbol(const FunctionMetaData &meta,
                           const std::shared_ptr<Function> &fn) {
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

inline std::shared_ptr<Value>
IREmitter::resolve_ptr(std::shared_ptr<Value> value, const SemType &expected,
                       const std::string &name_hint) {
  auto expectedTy = context->resolveType(expected);
  auto valPtr = std::dynamic_pointer_cast<const PointerType>(value->type());
  auto expPtr = std::dynamic_pointer_cast<const PointerType>(expectedTy);

  if (expPtr) {
    if (valPtr) {
      LOG_DEBUG("[IREmitter] resolve_ptr: value already a pointer");
      return value;
    }
    auto tmp = current_block_->append<AllocaInst>(
        expPtr->pointee(), nullptr, 0, name_hint.empty() ? "tmp" : name_hint);
    current_block_->append<StoreInst>(value, tmp);
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
  if (!decl.self_param) {
    LOG_ERROR("[IREmitter] self parameter requested but not present");
    throw IRException("self parameter requested but not present");
  }
  SemType base = decl.self_param->explicit_type.has_value()
                     ? ScopeNode::resolve_type(*decl.self_param->explicit_type,
                                               current_scope_node)
                     : SemType::named(owner);
  if (decl.self_param->is_reference) {
    base = SemType::reference(base, decl.self_param->is_mutable);
  }
  return base;
}

inline std::shared_ptr<Function>
IREmitter::emit_function(const FunctionMetaData &meta, const FunctionDecl &node,
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

  auto retTy = context->resolveType(meta.return_type);
  auto fnTy = std::make_shared<FunctionType>(retTy, paramTyp, false);

  auto fn = create_function(mangled_name, fnTy, !node.body.has_value(), &meta);
  name_mangle_[&node] = mangled_name;

  if (!node.body.has_value()) {
    LOG_DEBUG("[IREmitter] emit_function: external/forward-declared function, "
              "skipping body");
    return fn;
  }

  auto saved_block = current_block_;
  functions_.push_back(fn);
  auto *previousScope = current_scope_node;
  current_block_ = fn->createBlock("entry");
  pushLocalScope();

  std::vector<std::string> paramNames;
  if (node.self_param) {
    paramNames.emplace_back("self");
  }
  for (const auto &p : meta.param_names) {
    if (auto *id = dynamic_cast<IdentifierPattern *>(p.get())) {
      paramNames.push_back(id->name);
    }
  }

  for (size_t i = 0; i < fn->args().size(); ++i) {
    std::string paramName = paramNames[i];
    fn->args()[i]->setName(paramName);
    auto slot =
        current_block_->append<AllocaInst>(paramTyp[i], nullptr, 0, paramName);
    current_block_->append<StoreInst>(fn->args()[i], slot);
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
    current_scope_node = previousScope;
  }

  popLocalScope();
  current_block_ = saved_block;
  functions_.pop_back();
  LOG_INFO("[IREmitter] Finished emitting function: " + meta.name);
  return fn;
}

inline std::shared_ptr<Function>
IREmitter::create_function(const std::string &name,
                           std::shared_ptr<FunctionType> ty, bool is_external,
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

} // namespace rc::ir
