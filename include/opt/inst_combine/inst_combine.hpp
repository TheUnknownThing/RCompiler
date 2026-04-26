
#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"

#include "opt/sccp/context.hpp"
#include "opt/utils/ir_utils.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace rc::opt {

class InstCombineContext {
public:
  explicit InstCombineContext(ConstantContext *const_ctx)
      : const_ctx_(const_ctx) {}

  std::shared_ptr<ir::ConstantInt> get_i32(int v, bool is_signed = true) {
    if (const_ctx_) {
      return const_ctx_->get_int_constant(v, is_signed);
    }
    return ir::ConstantInt::get_i32(static_cast<std::uint32_t>(v), is_signed);
  }

  std::shared_ptr<ir::ConstantInt> get_i1(bool v) {
    if (const_ctx_) {
      return const_ctx_->get_bool_constant(v);
    }
    return ir::ConstantInt::get_i1(v);
  }

private:
  ConstantContext *const_ctx_{nullptr};
};

class InstCombineRule {
public:
  virtual ~InstCombineRule() = default;
  virtual bool try_apply(ir::Instruction &inst, InstCombineContext &ctx) = 0;
};

class InstCombinePass {
public:
  explicit InstCombinePass(ConstantContext *const_ctx = nullptr)
      : const_ctx_(const_ctx) {
    register_default_rules();
  }

  void add_rule(std::unique_ptr<InstCombineRule> rule) {
    if (rule) {
      rules_.push_back(std::move(rule));
    }
  }

  void run(ir::Module &module) {
    InstCombineContext ctx(const_ctx_);
    for (const auto &fn : module.functions()) {
      if (!fn || fn->is_external()) {
        continue;
      }
      run_on_function(*fn, ctx);
    }
  }

private:
  ConstantContext *const_ctx_{nullptr};
  std::vector<std::unique_ptr<InstCombineRule>> rules_;

  void register_default_rules();

  void run_on_function(ir::Function &fn, InstCombineContext &ctx) {
    bool changed = true;
    int iters = 0;
    while (changed && iters++ < 10) {
      changed = false;
      for (auto &bb : fn.blocks()) {
        if (!bb) {
          continue;
        }
        changed |= run_on_basic_block(*bb, ctx);
      }
    }
  }

  bool run_on_basic_block(ir::BasicBlock &bb, InstCombineContext &ctx) {
    bool changed = false;

    for (std::size_t i = 0; i < bb.instructions().size();) {
      auto inst_sp = bb.instructions()[i];
      auto *inst = inst_sp.get();
      if (!inst) {
        ++i;
        continue;
      }

      bool local_changed = false;
      for (auto &rule : rules_) {
        if (!rule) {
          continue;
        }
        if (rule->try_apply(*inst, ctx)) {
          local_changed = true;
          changed = true;
          break;
        }
      }

      if (local_changed) {
        if (i < bb.instructions().size() &&
            bb.instructions()[i].get() != inst) {
          continue;
        }
      }

      ++i;
    }

    return changed;
  }

public:
  static bool replace_inst_with_value(ir::Instruction &inst,
                                   ir::Value *replacement) {
    if (!replacement) {
      return false;
    }
    auto *bb = inst.parent();
    if (!bb) {
      return false;
    }
    utils::replace_all_uses_with(inst, replacement);
    if (!utils::erase_instruction(*bb, &inst)) {
      return false;
    }
    return true;
  }
};

class BinaryConstFoldRule final : public InstCombineRule {
public:
  bool try_apply(ir::Instruction &inst, InstCombineContext &ctx) override {
    auto *bin = dynamic_cast<ir::BinaryOpInst *>(&inst);
    if (!bin) {
      return false;
    }

    auto lhs_c = utils::as_const_int(bin->lhs().get());
    auto rhs_c = utils::as_const_int(bin->rhs().get());
    if (!lhs_c || !rhs_c) {
      return false;
    }

    const std::uint64_t a = lhs_c->value();
    const std::uint64_t b = rhs_c->value();
    std::optional<std::uint64_t> folded;

    switch (bin->op()) {
    case ir::BinaryOpKind::ADD:
      folded = a + b;
      break;
    case ir::BinaryOpKind::SUB:
      folded = a - b;
      break;
    case ir::BinaryOpKind::MUL:
      folded = a * b;
      break;
    case ir::BinaryOpKind::UDIV:
    case ir::BinaryOpKind::SDIV:
      if (b == 0) {
        return false;
      }
      folded = a / b;
      break;
    case ir::BinaryOpKind::UREM:
    case ir::BinaryOpKind::SREM:
      if (b == 0) {
        return false;
      }
      folded = a % b;
      break;
    case ir::BinaryOpKind::AND:
      folded = a & b;
      break;
    case ir::BinaryOpKind::OR:
      folded = a | b;
      break;
    case ir::BinaryOpKind::XOR:
      folded = a ^ b;
      break;
    case ir::BinaryOpKind::SHL:
      folded = a << b;
      break;
    case ir::BinaryOpKind::LSHR:
    case ir::BinaryOpKind::ASHR:
      folded = a >> b;
      break;
    }

    if (!folded.has_value()) {
      return false;
    }

    auto c = ctx.get_i32(static_cast<int>(*folded));
    return InstCombinePass::replace_inst_with_value(inst, c.get());
  }
};

class BinaryIdentityRule final : public InstCombineRule {
public:
  bool try_apply(ir::Instruction &inst, InstCombineContext &ctx) override {
    auto *bin = dynamic_cast<ir::BinaryOpInst *>(&inst);
    if (!bin) {
      return false;
    }

    auto *lhs = bin->lhs().get();
    auto *rhs = bin->rhs().get();

    switch (bin->op()) {
    case ir::BinaryOpKind::ADD:
      if (utils::is_const_int(rhs, 0)) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      if (utils::is_const_int(lhs, 0)) {
        return InstCombinePass::replace_inst_with_value(inst, rhs);
      }
      return false;
    case ir::BinaryOpKind::SUB:
      if (utils::is_const_int(rhs, 0)) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      if (lhs == rhs) {
        auto c0 = ctx.get_i32(0);
        return InstCombinePass::replace_inst_with_value(inst, c0.get());
      }
      return false;
    case ir::BinaryOpKind::MUL:
      if (utils::is_const_int(rhs, 1)) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      if (utils::is_const_int(lhs, 1)) {
        return InstCombinePass::replace_inst_with_value(inst, rhs);
      }
      if (utils::is_const_int(rhs, 0) || utils::is_const_int(lhs, 0)) {
        auto c0 = ctx.get_i32(0);
        return InstCombinePass::replace_inst_with_value(inst, c0.get());
      }
      return false;
    case ir::BinaryOpKind::AND:
      if (utils::is_const_int(rhs, 0) || utils::is_const_int(lhs, 0)) {
        auto c0 = ctx.get_i32(0);
        return InstCombinePass::replace_inst_with_value(inst, c0.get());
      }
      if (lhs == rhs) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      return false;
    case ir::BinaryOpKind::OR:
      if (utils::is_const_int(rhs, 0)) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      if (utils::is_const_int(lhs, 0)) {
        return InstCombinePass::replace_inst_with_value(inst, rhs);
      }
      if (lhs == rhs) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      return false;
    case ir::BinaryOpKind::XOR:
      if (utils::is_const_int(rhs, 0)) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      if (utils::is_const_int(lhs, 0)) {
        return InstCombinePass::replace_inst_with_value(inst, rhs);
      }
      if (lhs == rhs) {
        auto c0 = ctx.get_i32(0);
        return InstCombinePass::replace_inst_with_value(inst, c0.get());
      }
      return false;
    case ir::BinaryOpKind::SDIV:
    case ir::BinaryOpKind::UDIV:
      if (utils::is_const_int(rhs, 1)) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      return false;
    case ir::BinaryOpKind::SREM:
    case ir::BinaryOpKind::UREM:
      if (utils::is_const_int(rhs, 1)) {
        auto c0 = ctx.get_i32(0);
        return InstCombinePass::replace_inst_with_value(inst, c0.get());
      }
      return false;
    case ir::BinaryOpKind::SHL:
    case ir::BinaryOpKind::LSHR:
    case ir::BinaryOpKind::ASHR:
      if (utils::is_const_int(rhs, 0)) {
        return InstCombinePass::replace_inst_with_value(inst, lhs);
      }
      return false;
    }
    return false;
  }
};

class StrengthReductionRule final : public InstCombineRule {
public:
  bool try_apply(ir::Instruction &inst, InstCombineContext &ctx) override {
    auto *bin = dynamic_cast<ir::BinaryOpInst *>(&inst);
    if (!bin) {
      return false;
    }

    auto bits = utils::int_bits(bin->type());
    if (!bits || *bits != 32) {
      return false;
    }

    switch (bin->op()) {
    case ir::BinaryOpKind::ADD:
      return fold_add(*bin, ctx);
    case ir::BinaryOpKind::MUL:
      return fold_mul(*bin, ctx);
    case ir::BinaryOpKind::UDIV:
      return fold_udiv(*bin, ctx);
    case ir::BinaryOpKind::UREM:
      return fold_urem(*bin, ctx);
    default:
      return false;
    }
  }

private:
  static bool is_pow2(std::uint64_t v) {
    return v != 0 && (v & (v - 1)) == 0;
  }

  static unsigned log2_exact(std::uint64_t v) {
    return static_cast<unsigned>(std::countr_zero(v));
  }

  static bool replace_with_new_inst(
      ir::BinaryOpInst &old_inst,
      const std::shared_ptr<ir::Instruction> &replacement) {
    if (!replacement) {
      return false;
    }
    utils::replace_all_uses_with(old_inst, replacement.get());
    return utils::erase_instruction(*old_inst.parent(), &old_inst);
  }

  static std::shared_ptr<ir::BinaryOpInst>
  insert_binary_before(ir::BinaryOpInst &pos, ir::BinaryOpKind op,
                       std::shared_ptr<ir::Value> lhs,
                       std::shared_ptr<ir::Value> rhs, ir::TypePtr ty,
                       std::string name = {}) {
    auto *bb = pos.parent();
    if (!bb) {
      return nullptr;
    }
    auto pos_sp = utils::find_shared_instruction(*bb, &pos);
    if (!pos_sp) {
      return nullptr;
    }
    return bb->insert_before<ir::BinaryOpInst>(pos_sp, op, std::move(lhs),
                                               std::move(rhs), std::move(ty),
                                               std::move(name));
  }

  static std::shared_ptr<ir::Value> shift_amount(InstCombineContext &ctx,
                                                 unsigned amount) {
    return ctx.get_i32(static_cast<int>(amount));
  }

  bool fold_add(ir::BinaryOpInst &bin, InstCombineContext &ctx) {
    auto lhs = bin.lhs();
    auto rhs = bin.rhs();
    if (lhs.get() != rhs.get()) {
      return false;
    }

    auto shl = insert_binary_before(bin, ir::BinaryOpKind::SHL, lhs,
                                    shift_amount(ctx, 1), bin.type(),
                                    bin.name());
    return replace_with_new_inst(bin, shl);
  }

  bool fold_mul(ir::BinaryOpInst &bin, InstCombineContext &ctx) {
    auto lhs_c = utils::as_const_int(bin.lhs().get());
    auto rhs_c = utils::as_const_int(bin.rhs().get());

    std::shared_ptr<ir::Value> value;
    std::uint64_t factor = 0;
    if (rhs_c && !lhs_c) {
      value = bin.lhs();
      factor = rhs_c->value();
    } else if (lhs_c && !rhs_c) {
      value = bin.rhs();
      factor = lhs_c->value();
    } else {
      return false;
    }

    if (factor > UINT32_MAX || factor <= 1) {
      return false;
    }

    if (is_pow2(factor)) {
      auto shl = insert_binary_before(bin, ir::BinaryOpKind::SHL, value,
                                      shift_amount(ctx, log2_exact(factor)),
                                      bin.type(), bin.name());
      return replace_with_new_inst(bin, shl);
    }

    if (is_pow2(factor - 1)) {
      auto shl = insert_binary_before(bin, ir::BinaryOpKind::SHL, value,
                                      shift_amount(ctx, log2_exact(factor - 1)),
                                      bin.type());
      if (!shl) {
        return false;
      }
      auto add = insert_binary_before(bin, ir::BinaryOpKind::ADD, shl, value,
                                      bin.type(), bin.name());
      return replace_with_new_inst(bin, add);
    }

    if (factor > 2 && is_pow2(factor + 1) && log2_exact(factor + 1) < 32) {
      auto shl = insert_binary_before(bin, ir::BinaryOpKind::SHL, value,
                                      shift_amount(ctx, log2_exact(factor + 1)),
                                      bin.type());
      if (!shl) {
        return false;
      }
      auto sub = insert_binary_before(bin, ir::BinaryOpKind::SUB, shl, value,
                                      bin.type(), bin.name());
      return replace_with_new_inst(bin, sub);
    }

    if (std::popcount(static_cast<std::uint32_t>(factor)) == 2) {
      const auto low = log2_exact(factor & (~factor + 1));
      const auto high = log2_exact(factor ^ (1ULL << low));

      std::shared_ptr<ir::Value> low_term = value;
      if (low != 0) {
        low_term = insert_binary_before(bin, ir::BinaryOpKind::SHL, value,
                                        shift_amount(ctx, low), bin.type());
      }
      auto high_term = insert_binary_before(bin, ir::BinaryOpKind::SHL, value,
                                            shift_amount(ctx, high), bin.type());
      if (!low_term || !high_term) {
        return false;
      }
      auto add = insert_binary_before(bin, ir::BinaryOpKind::ADD, low_term,
                                      high_term, bin.type(), bin.name());
      return replace_with_new_inst(bin, add);
    }

    return false;
  }

  bool fold_udiv(ir::BinaryOpInst &bin, InstCombineContext &ctx) {
    auto rhs_c = utils::as_const_int(bin.rhs().get());
    if (!rhs_c) {
      return false;
    }
    const auto divisor = rhs_c->value();
    if (divisor <= 1 || divisor > UINT32_MAX || !is_pow2(divisor)) {
      return false;
    }

    auto lshr = insert_binary_before(bin, ir::BinaryOpKind::LSHR, bin.lhs(),
                                     shift_amount(ctx, log2_exact(divisor)),
                                     bin.type(), bin.name());
    return replace_with_new_inst(bin, lshr);
  }

  bool fold_urem(ir::BinaryOpInst &bin, InstCombineContext &ctx) {
    auto rhs_c = utils::as_const_int(bin.rhs().get());
    if (!rhs_c) {
      return false;
    }
    const auto divisor = rhs_c->value();
    if (divisor <= 1 || divisor > UINT32_MAX || !is_pow2(divisor)) {
      return false;
    }

    auto mask = ctx.get_i32(static_cast<int>(divisor - 1), false);
    auto and_inst = insert_binary_before(bin, ir::BinaryOpKind::AND, bin.lhs(),
                                         mask, bin.type(), bin.name());
    return replace_with_new_inst(bin, and_inst);
  }
};

class ICmpFoldRule final : public InstCombineRule {
public:
  bool try_apply(ir::Instruction &inst, InstCombineContext &ctx) override {
    auto *icmp = dynamic_cast<ir::ICmpInst *>(&inst);
    if (!icmp) {
      return false;
    }

    auto *lhs = icmp->lhs().get();
    auto *rhs = icmp->rhs().get();

    // Same operand folds.
    if (lhs == rhs) {
      bool res = false;
      switch (icmp->pred()) {
      case ir::ICmpPred::EQ:
      case ir::ICmpPred::UGE:
      case ir::ICmpPred::ULE:
      case ir::ICmpPred::SGE:
      case ir::ICmpPred::SLE:
        res = true;
        break;
      case ir::ICmpPred::NE:
      case ir::ICmpPred::UGT:
      case ir::ICmpPred::ULT:
      case ir::ICmpPred::SGT:
      case ir::ICmpPred::SLT:
        res = false;
        break;
      }
      auto c = ctx.get_i1(res);
      return InstCombinePass::replace_inst_with_value(inst, c.get());
    }

    // Constant folding.
    auto lhs_c = utils::as_const_int(lhs);
    auto rhs_c = utils::as_const_int(rhs);
    if (!lhs_c || !rhs_c) {
      return false;
    }
    const std::uint64_t a = lhs_c->value();
    const std::uint64_t b = rhs_c->value();

    bool res = false;
    switch (icmp->pred()) {
    case ir::ICmpPred::EQ:
      res = (a == b);
      break;
    case ir::ICmpPred::NE:
      res = (a != b);
      break;
    case ir::ICmpPred::UGT:
      res = (a > b);
      break;
    case ir::ICmpPred::UGE:
      res = (a >= b);
      break;
    case ir::ICmpPred::ULT:
      res = (a < b);
      break;
    case ir::ICmpPred::ULE:
      res = (a <= b);
      break;
    case ir::ICmpPred::SGT:
      res = (static_cast<std::int64_t>(a) > static_cast<std::int64_t>(b));
      break;
    case ir::ICmpPred::SGE:
      res = (static_cast<std::int64_t>(a) >= static_cast<std::int64_t>(b));
      break;
    case ir::ICmpPred::SLT:
      res = (static_cast<std::int64_t>(a) < static_cast<std::int64_t>(b));
      break;
    case ir::ICmpPred::SLE:
      res = (static_cast<std::int64_t>(a) <= static_cast<std::int64_t>(b));
      break;
    }

    auto c = ctx.get_i1(res);
    return InstCombinePass::replace_inst_with_value(inst, c.get());
  }
};

// select(c, x, x) -> x; select(true/false, t, f) -> t/f.
class SelectFoldRule final : public InstCombineRule {
public:
  bool try_apply(ir::Instruction &inst, InstCombineContext &ctx) override {
    auto *sel = dynamic_cast<ir::SelectInst *>(&inst);
    if (!sel) {
      return false;
    }

    auto *cond = sel->cond().get();
    auto *t = sel->if_true().get();
    auto *f = sel->if_false().get();

    if (t == f) {
      return InstCombinePass::replace_inst_with_value(inst, t);
    }

    if (auto c = utils::as_const_int(cond)) {
      if (c->value() != 0) {
        return InstCombinePass::replace_inst_with_value(inst, t);
      }
      return InstCombinePass::replace_inst_with_value(inst, f);
    }
    (void)ctx;
    return false;
  }
};

class TrivialCastRule final : public InstCombineRule {
public:
  bool try_apply(ir::Instruction &inst, InstCombineContext &ctx) override {
    (void)ctx;
    if (auto *z = dynamic_cast<ir::ZExtInst *>(&inst)) {
      auto src_bits = utils::int_bits(z->source()->type());
      auto dst_bits = utils::int_bits(z->dest_type());
      if (src_bits && dst_bits && *src_bits == *dst_bits) {
        return InstCombinePass::replace_inst_with_value(inst, z->source().get());
      }
      return false;
    }
    if (auto *s = dynamic_cast<ir::SExtInst *>(&inst)) {
      auto src_bits = utils::int_bits(s->source()->type());
      auto dst_bits = utils::int_bits(s->dest_type());
      if (src_bits && dst_bits && *src_bits == *dst_bits) {
        return InstCombinePass::replace_inst_with_value(inst, s->source().get());
      }
      return false;
    }
    if (auto *t = dynamic_cast<ir::TruncInst *>(&inst)) {
      auto src_bits = utils::int_bits(t->source()->type());
      auto dst_bits = utils::int_bits(t->dest_type());
      if (src_bits && dst_bits && *src_bits == *dst_bits) {
        return InstCombinePass::replace_inst_with_value(inst, t->source().get());
      }

      // trunc(zext(x)) -> x if trunc type matches x type
      if (auto *z = dynamic_cast<ir::ZExtInst *>(t->source().get())) {
        if (z->source() && z->source()->type() == t->dest_type()) {
          return InstCombinePass::replace_inst_with_value(inst, z->source().get());
        }
      }
      // trunc(sext(x)) -> x if trunc type matches x type
      if (auto *s = dynamic_cast<ir::SExtInst *>(t->source().get())) {
        if (s->source() && s->source()->type() == t->dest_type()) {
          return InstCombinePass::replace_inst_with_value(inst, s->source().get());
        }
      }

      return false;
    }
    return false;
  }
};

} // namespace rc::opt
