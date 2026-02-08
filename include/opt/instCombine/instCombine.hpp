
#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "opt/sccp/context.hpp"
#include "opt/utils/ir_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace rc::opt {

class InstCombineContext {
public:
  explicit InstCombineContext(ConstantContext *constCtx)
      : constCtx_(constCtx) {}

  std::shared_ptr<ir::ConstantInt> getI32(int v, bool isSigned = true) {
    if (constCtx_) {
      return constCtx_->getIntConstant(v, isSigned);
    }
    return ir::ConstantInt::getI32(static_cast<std::uint32_t>(v), isSigned);
  }

  std::shared_ptr<ir::ConstantInt> getI1(bool v) {
    if (constCtx_) {
      return constCtx_->getBoolConstant(v);
    }
    return ir::ConstantInt::getI1(v);
  }

private:
  ConstantContext *constCtx_{nullptr};
};

class InstCombineRule {
public:
  virtual ~InstCombineRule() = default;
  virtual bool tryApply(ir::Instruction &inst, InstCombineContext &ctx) = 0;
};

class InstCombinePass {
public:
  explicit InstCombinePass(ConstantContext *constCtx = nullptr)
      : constCtx_(constCtx) {
    registerDefaultRules();
  }

  void addRule(std::unique_ptr<InstCombineRule> rule) {
    if (rule) {
      rules_.push_back(std::move(rule));
    }
  }

  void run(ir::Module &module) {
    InstCombineContext ctx(constCtx_);
    for (const auto &fn : module.functions()) {
      if (!fn || fn->isExternal()) {
        continue;
      }
      runOnFunction(*fn, ctx);
    }
  }

private:
  ConstantContext *constCtx_{nullptr};
  std::vector<std::unique_ptr<InstCombineRule>> rules_;

  void registerDefaultRules();

  void runOnFunction(ir::Function &fn, InstCombineContext &ctx) {
    bool changed = true;
    int iters = 0;
    while (changed && iters++ < 10) {
      changed = false;
      for (auto &bb : fn.blocks()) {
        if (!bb) {
          continue;
        }
        changed |= runOnBasicBlock(*bb, ctx);
      }
    }
  }

  bool runOnBasicBlock(ir::BasicBlock &bb, InstCombineContext &ctx) {
    bool changed = false;

    for (std::size_t i = 0; i < bb.instructions().size();) {
      auto instSP = bb.instructions()[i];
      auto *inst = instSP.get();
      if (!inst) {
        ++i;
        continue;
      }

      bool localChanged = false;
      for (auto &rule : rules_) {
        if (!rule) {
          continue;
        }
        if (rule->tryApply(*inst, ctx)) {
          localChanged = true;
          changed = true;
          break;
        }
      }

      if (localChanged) {
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
  static bool replaceInstWithValue(ir::Instruction &inst,
                                   ir::Value *replacement) {
    if (!replacement) {
      return false;
    }
    auto *bb = inst.parent();
    if (!bb) {
      return false;
    }
    utils::replaceAllUsesWith(inst, replacement);
    if (!utils::eraseInstruction(*bb, &inst)) {
      return false;
    }
    return true;
  }
};

class BinaryConstFoldRule final : public InstCombineRule {
public:
  bool tryApply(ir::Instruction &inst, InstCombineContext &ctx) override {
    auto *bin = dynamic_cast<ir::BinaryOpInst *>(&inst);
    if (!bin) {
      return false;
    }

    auto lhsC = utils::asConstInt(bin->lhs().get());
    auto rhsC = utils::asConstInt(bin->rhs().get());
    if (!lhsC || !rhsC) {
      return false;
    }

    const std::uint64_t a = lhsC->value();
    const std::uint64_t b = rhsC->value();
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

    auto c = ctx.getI32(static_cast<int>(*folded));
    return InstCombinePass::replaceInstWithValue(inst, c.get());
  }
};

class BinaryIdentityRule final : public InstCombineRule {
public:
  bool tryApply(ir::Instruction &inst, InstCombineContext &ctx) override {
    auto *bin = dynamic_cast<ir::BinaryOpInst *>(&inst);
    if (!bin) {
      return false;
    }

    auto *lhs = bin->lhs().get();
    auto *rhs = bin->rhs().get();

    switch (bin->op()) {
    case ir::BinaryOpKind::ADD:
      if (utils::isConstInt(rhs, 0)) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      if (utils::isConstInt(lhs, 0)) {
        return InstCombinePass::replaceInstWithValue(inst, rhs);
      }
      return false;
    case ir::BinaryOpKind::SUB:
      if (utils::isConstInt(rhs, 0)) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      if (lhs == rhs) {
        auto c0 = ctx.getI32(0);
        return InstCombinePass::replaceInstWithValue(inst, c0.get());
      }
      return false;
    case ir::BinaryOpKind::MUL:
      if (utils::isConstInt(rhs, 1)) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      if (utils::isConstInt(lhs, 1)) {
        return InstCombinePass::replaceInstWithValue(inst, rhs);
      }
      if (utils::isConstInt(rhs, 0) || utils::isConstInt(lhs, 0)) {
        auto c0 = ctx.getI32(0);
        return InstCombinePass::replaceInstWithValue(inst, c0.get());
      }
      return false;
    case ir::BinaryOpKind::AND:
      if (utils::isConstInt(rhs, 0) || utils::isConstInt(lhs, 0)) {
        auto c0 = ctx.getI32(0);
        return InstCombinePass::replaceInstWithValue(inst, c0.get());
      }
      if (lhs == rhs) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      return false;
    case ir::BinaryOpKind::OR:
      if (utils::isConstInt(rhs, 0)) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      if (utils::isConstInt(lhs, 0)) {
        return InstCombinePass::replaceInstWithValue(inst, rhs);
      }
      if (lhs == rhs) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      return false;
    case ir::BinaryOpKind::XOR:
      if (utils::isConstInt(rhs, 0)) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      if (utils::isConstInt(lhs, 0)) {
        return InstCombinePass::replaceInstWithValue(inst, rhs);
      }
      if (lhs == rhs) {
        auto c0 = ctx.getI32(0);
        return InstCombinePass::replaceInstWithValue(inst, c0.get());
      }
      return false;
    case ir::BinaryOpKind::SDIV:
    case ir::BinaryOpKind::UDIV:
      if (utils::isConstInt(rhs, 1)) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      return false;
    case ir::BinaryOpKind::SREM:
    case ir::BinaryOpKind::UREM:
      if (utils::isConstInt(rhs, 1)) {
        auto c0 = ctx.getI32(0);
        return InstCombinePass::replaceInstWithValue(inst, c0.get());
      }
      return false;
    case ir::BinaryOpKind::SHL:
    case ir::BinaryOpKind::LSHR:
    case ir::BinaryOpKind::ASHR:
      if (utils::isConstInt(rhs, 0)) {
        return InstCombinePass::replaceInstWithValue(inst, lhs);
      }
      return false;
    }
    return false;
  }
};

class ICmpFoldRule final : public InstCombineRule {
public:
  bool tryApply(ir::Instruction &inst, InstCombineContext &ctx) override {
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
      auto c = ctx.getI1(res);
      return InstCombinePass::replaceInstWithValue(inst, c.get());
    }

    // Constant folding.
    auto lhsC = utils::asConstInt(lhs);
    auto rhsC = utils::asConstInt(rhs);
    if (!lhsC || !rhsC) {
      return false;
    }
    const std::uint64_t a = lhsC->value();
    const std::uint64_t b = rhsC->value();

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

    auto c = ctx.getI1(res);
    return InstCombinePass::replaceInstWithValue(inst, c.get());
  }
};

// select(c, x, x) -> x; select(true/false, t, f) -> t/f.
class SelectFoldRule final : public InstCombineRule {
public:
  bool tryApply(ir::Instruction &inst, InstCombineContext &ctx) override {
    auto *sel = dynamic_cast<ir::SelectInst *>(&inst);
    if (!sel) {
      return false;
    }

    auto *cond = sel->cond().get();
    auto *t = sel->ifTrue().get();
    auto *f = sel->ifFalse().get();

    if (t == f) {
      return InstCombinePass::replaceInstWithValue(inst, t);
    }

    if (auto c = utils::asConstInt(cond)) {
      if (c->value() != 0) {
        return InstCombinePass::replaceInstWithValue(inst, t);
      }
      return InstCombinePass::replaceInstWithValue(inst, f);
    }
    (void)ctx;
    return false;
  }
};

class TrivialCastRule final : public InstCombineRule {
public:
  bool tryApply(ir::Instruction &inst, InstCombineContext &ctx) override {
    (void)ctx;
    if (auto *z = dynamic_cast<ir::ZExtInst *>(&inst)) {
      auto srcBits = utils::intBits(z->source()->type());
      auto dstBits = utils::intBits(z->destType());
      if (srcBits && dstBits && *srcBits == *dstBits) {
        return InstCombinePass::replaceInstWithValue(inst, z->source().get());
      }
      return false;
    }
    if (auto *s = dynamic_cast<ir::SExtInst *>(&inst)) {
      auto srcBits = utils::intBits(s->source()->type());
      auto dstBits = utils::intBits(s->destType());
      if (srcBits && dstBits && *srcBits == *dstBits) {
        return InstCombinePass::replaceInstWithValue(inst, s->source().get());
      }
      return false;
    }
    if (auto *t = dynamic_cast<ir::TruncInst *>(&inst)) {
      auto srcBits = utils::intBits(t->source()->type());
      auto dstBits = utils::intBits(t->destType());
      if (srcBits && dstBits && *srcBits == *dstBits) {
        return InstCombinePass::replaceInstWithValue(inst, t->source().get());
      }

      // trunc(zext(x)) -> x if trunc type matches x type
      if (auto *z = dynamic_cast<ir::ZExtInst *>(t->source().get())) {
        if (z->source() && z->source()->type() == t->destType()) {
          return InstCombinePass::replaceInstWithValue(inst, z->source().get());
        }
      }
      // trunc(sext(x)) -> x if trunc type matches x type
      if (auto *s = dynamic_cast<ir::SExtInst *>(t->source().get())) {
        if (s->source() && s->source()->type() == t->destType()) {
          return InstCombinePass::replaceInstWithValue(inst, s->source().get());
        }
      }

      return false;
    }
    return false;
  }
};

inline void InstCombinePass::registerDefaultRules() {
  addRule(std::make_unique<BinaryConstFoldRule>());
  addRule(std::make_unique<BinaryIdentityRule>());
  addRule(std::make_unique<ICmpFoldRule>());
  addRule(std::make_unique<SelectFoldRule>());
  addRule(std::make_unique<TrivialCastRule>());
}

} // namespace rc::opt
