#pragma once

#include "ir/instructions/binary.hpp"
#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/misc.hpp"
#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include "context.hpp"
#include "opt/base/baseVisitor.hpp"
#include "opt/utils/cfgPrettyPrint.hpp"

#include "utils/logger.hpp"

namespace rc::opt {

enum class LatticeValueKind {
  UNDEF,
  CONSTANT,
  OVERDEF
}; // overdef means variable

class SCCPVisitor : public IRBaseVisitor {
public:
  virtual ~SCCPVisitor() = default;
  explicit SCCPVisitor(ConstantContext *context) : context_(context) {}

  void run(ir::Module &module);

  void visit(ir::Value &value) override;

  void visit(ir::Function &function) override;
  void visit(ir::BasicBlock &basicBlock) override;

  void visit(ir::BinaryOpInst &binaryOpInst) override;
  void visit(ir::BranchInst &branchInst) override;
  void visit(ir::UnreachableInst &unreachableInst) override;
  void visit(ir::ReturnInst &returnInst) override;
  void visit(ir::AllocaInst &allocaInst) override;
  void visit(ir::LoadInst &loadInst) override;
  void visit(ir::StoreInst &storeInst) override;
  void visit(ir::GetElementPtrInst &getElementPtrInst) override;
  void visit(ir::ICmpInst &icmpInst) override;
  void visit(ir::SExtInst &sextInst) override;
  void visit(ir::ZExtInst &zextInst) override;
  void visit(ir::TruncInst &truncInst) override;
  void visit(ir::CallInst &callInst) override;
  void visit(ir::PhiInst &phiInst) override;
  void visit(ir::SelectInst &selectInst) override;

private:
  std::vector<ir::Instruction *> instructionWorklist_;
  std::vector<std::pair<const ir::BasicBlock *, const ir::BasicBlock *>> edges_;

  std::unordered_map<ir::Value *, LatticeValueKind> latticeValues_;
  std::unordered_map<ir::Value *, std::shared_ptr<ir::Constant>>
      constantValues_;
  std::unordered_set<const ir::BasicBlock *> executableBlocks_;

  ConstantContext *context_{nullptr};

  LatticeValueKind getLatticeValue(ir::Value *value) {
    auto it = latticeValues_.find(value);
    if (it != latticeValues_.end()) {
      return it->second;
    }
    return LatticeValueKind::UNDEF;
  }

  LatticeValueKind evaluateKind(ir::Value *value_1, ir::Value *value_2) {
    auto kind1 = getLatticeValue(value_1);
    auto kind2 = getLatticeValue(value_2);

    if (kind1 == LatticeValueKind::OVERDEF ||
        kind2 == LatticeValueKind::OVERDEF) {
      return LatticeValueKind::OVERDEF;
    }
    if (kind1 == LatticeValueKind::UNDEF || kind2 == LatticeValueKind::UNDEF) {
      return LatticeValueKind::UNDEF;
    }
    return LatticeValueKind::CONSTANT;
  }

  std::pair<LatticeValueKind, std::shared_ptr<ir::Constant>>
  mergePHIValues(LatticeValueKind kind1, ir::Value *value1,
                 LatticeValueKind kind2, ir::Value *value2) {
    if (kind1 == LatticeValueKind::UNDEF && kind2 == LatticeValueKind::UNDEF) {
      return {LatticeValueKind::UNDEF, nullptr};
    } else if (kind1 == LatticeValueKind::OVERDEF ||
               kind2 == LatticeValueKind::OVERDEF) {
      return {LatticeValueKind::OVERDEF, nullptr};
    } else if (kind1 == LatticeValueKind::CONSTANT &&
               kind2 == LatticeValueKind::CONSTANT) {
      auto const1 = constantValues_[value1];
      auto const2 = constantValues_[value2];
      if (const1->equals(*const2)) {
        return {LatticeValueKind::CONSTANT, const1};
      } else {
        return {LatticeValueKind::OVERDEF, nullptr};
      }
    } else if (kind1 == LatticeValueKind::CONSTANT &&
               kind2 == LatticeValueKind::UNDEF) {
      return {LatticeValueKind::CONSTANT, constantValues_[value1]};
    } else if (kind1 == LatticeValueKind::UNDEF &&
               kind2 == LatticeValueKind::CONSTANT) {
      return {LatticeValueKind::CONSTANT, constantValues_[value2]};
    } else {
      return {LatticeValueKind::OVERDEF, nullptr};
    }
  }
};

inline void SCCPVisitor::run(ir::Module &module) {
  for (const auto &function : module.functions()) {
    instructionWorklist_.clear();
    edges_.clear();
    latticeValues_.clear();
    constantValues_.clear();
    executableBlocks_.clear();

    visit(*function);
  }
}

inline void SCCPVisitor::visit(ir::Value &value) {
  if (auto module = dynamic_cast<ir::Module *>(&value)) {
    run(*module);
  } else if (auto function = dynamic_cast<ir::Function *>(&value)) {
    visit(*function);
  } else if (auto basicBlock = dynamic_cast<ir::BasicBlock *>(&value)) {
    visit(*basicBlock);
  } else if (auto branch = dynamic_cast<ir::BranchInst *>(&value)) {
    visit(*branch);
  } else if (auto ret = dynamic_cast<ir::ReturnInst *>(&value)) {
    visit(*ret);
  } else if (auto binOp = dynamic_cast<ir::BinaryOpInst *>(&value)) {
    visit(*binOp);
  } else if (auto load = dynamic_cast<ir::LoadInst *>(&value)) {
    visit(*load);
  } else if (auto store = dynamic_cast<ir::StoreInst *>(&value)) {
    visit(*store);
  } else if (auto gep = dynamic_cast<ir::GetElementPtrInst *>(&value)) {
    visit(*gep);
  } else if (auto icmp = dynamic_cast<ir::ICmpInst *>(&value)) {
    visit(*icmp);
  } else if (auto sext = dynamic_cast<ir::SExtInst *>(&value)) {
    visit(*sext);
  } else if (auto zext = dynamic_cast<ir::ZExtInst *>(&value)) {
    visit(*zext);
  } else if (auto trunc = dynamic_cast<ir::TruncInst *>(&value)) {
    visit(*trunc);
  } else if (auto call = dynamic_cast<ir::CallInst *>(&value)) {
    visit(*call);
  } else if (auto phi = dynamic_cast<ir::PhiInst *>(&value)) {
    visit(*phi);
  } else if (auto select = dynamic_cast<ir::SelectInst *>(&value)) {
    visit(*select);
  }
}

inline void SCCPVisitor::visit(ir::Function &function) {
  LOG_DEBUG("Starting SCCP on function: " + function.name());
  //   executableBlocks_.insert(function.blocks().front().get());
  auto initBlock =
      std::make_shared<ir::BasicBlock>("__sccp_init_block__", &function);
  edges_.push_back(
      std::make_pair(initBlock.get(), function.blocks().front().get()));

  for (auto arg : function.args()) {
    latticeValues_[arg.get()] = LatticeValueKind::OVERDEF;
  }

  while (!edges_.empty() || !instructionWorklist_.empty()) {
    // pop controlFlowWorklist first
    while (!edges_.empty()) {
      auto edge = edges_.back();
      edges_.pop_back();
      auto *fromBB = edge.first;
      auto *toBB = edge.second;
      if (executableBlocks_.count(toBB) == 0) {
        executableBlocks_.insert(toBB);
        for (const auto &inst : toBB->instructions()) {
          if (!inst || dynamic_cast<ir::UnreachableInst *>(inst.get())) {
            break;
          }
          instructionWorklist_.push_back(inst.get());
        }

        if (utils::detail::successors(*toBB).size() == 1) {
          auto *succ = utils::detail::successors(*toBB).front();
          edges_.push_back(std::make_pair(toBB, succ));
        }
      }

      // process PHI nodes
      for (const auto &inst : toBB->instructions()) {
        if (!inst || dynamic_cast<ir::UnreachableInst *>(inst.get())) {
          break;
        }
        if (auto *phi = dynamic_cast<ir::PhiInst *>(inst.get())) {
          instructionWorklist_.push_back(phi);
        } else {
          break; // PHI nodes are always at the beginning
        }
      }
    }

    while (!instructionWorklist_.empty()) {
      auto *inst = instructionWorklist_.back();
      instructionWorklist_.pop_back();
      visit(*inst);
    }
  }
}

inline void SCCPVisitor::visit(ir::BasicBlock &basicBlock) {
  LOG_DEBUG("SCCP visiting basic block: " + basicBlock.name());
  for (const auto &inst : basicBlock.instructions()) {
    if (!inst || dynamic_cast<ir::UnreachableInst *>(inst.get())) {
      break;
    }
    instructionWorklist_.push_back(inst.get());
  }
}

inline void SCCPVisitor::visit(ir::BinaryOpInst &binaryOpInst) {
  LOG_DEBUG("SCCP visiting binary op: " + binaryOpInst.name());
  auto kind = evaluateKind(binaryOpInst.lhs().get(), binaryOpInst.rhs().get());
  auto prev = getLatticeValue(&binaryOpInst);
  if (kind != prev) {
    latticeValues_[&binaryOpInst] = kind;
    // add users to worklist
    for (auto *user : binaryOpInst.getUses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instructionWorklist_.push_back(inst);
      }
    }
  }
}

inline void SCCPVisitor::visit(ir::BranchInst &branchInst) {
  LOG_DEBUG("SCCP visiting branch instruction");
  if (branchInst.isConditional()) {
    /// TODO: evaluate condition
  } else {
    auto *targetBB = branchInst.dest().get();
    edges_.push_back(std::make_pair(branchInst.parent(), targetBB));
  }
}

inline void SCCPVisitor::visit(ir::UnreachableInst &) {
  LOG_DEBUG("SCCP visiting unreachable instruction");
  // no-op
}

inline void SCCPVisitor::visit(ir::ReturnInst &) {
  LOG_DEBUG("SCCP visiting return instruction");
  // no-op
}

inline void SCCPVisitor::visit(ir::AllocaInst &allocaInst) {
  LOG_DEBUG("SCCP visiting alloca instruction");
  // overdef
  auto prev = getLatticeValue(&allocaInst);
  if (prev != LatticeValueKind::OVERDEF) {
    latticeValues_[&allocaInst] = LatticeValueKind::OVERDEF;
    // add users to worklist
    for (auto *user : allocaInst.getUses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instructionWorklist_.push_back(inst);
      }
    }
  }
}

inline void SCCPVisitor::visit(ir::LoadInst &loadInst) {
  LOG_DEBUG("SCCP visiting load instruction");
  auto prev = getLatticeValue(&loadInst);
  if (auto ptr = dynamic_cast<ir::ConstantPtr *>(loadInst.pointer().get())) {
    /// TODO: load constant value
    return;
  } else {
    // overdef
    if (prev != LatticeValueKind::OVERDEF) {
      latticeValues_[&loadInst] = LatticeValueKind::OVERDEF;
      // add users to worklist
      for (auto *user : loadInst.getUses()) {
        if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
          instructionWorklist_.push_back(inst);
        }
      }
    }
  }
}

inline void SCCPVisitor::visit(ir::StoreInst &) {
  LOG_DEBUG("SCCP visiting store instruction");
  // no-op
}

inline void SCCPVisitor::visit(ir::GetElementPtrInst &getElementPtrInst) {
  LOG_DEBUG("SCCP visiting getelementptr instruction");
  /// TODO: could be constant folded
}

inline void SCCPVisitor::visit(ir::ICmpInst &icmpInst) {
  LOG_DEBUG("SCCP visiting icmp instruction");
  auto kind = evaluateKind(icmpInst.lhs().get(), icmpInst.rhs().get());
  auto prev = getLatticeValue(&icmpInst);
  if (kind != prev) {
    latticeValues_[&icmpInst] = kind;
    // add users to worklist
    for (auto *user : icmpInst.getUses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instructionWorklist_.push_back(inst);
      }
    }
  }
}

inline void SCCPVisitor::visit(ir::SExtInst &sextInst) {
  LOG_DEBUG("SCCP visiting sext instruction");
  auto kind = getLatticeValue(sextInst.source().get());
  auto prev = getLatticeValue(&sextInst);
  if (kind != prev) {
    latticeValues_[&sextInst] = kind;
    // add users to worklist
    for (auto *user : sextInst.getUses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instructionWorklist_.push_back(inst);
      }
    }
  }
}

inline void SCCPVisitor::visit(ir::ZExtInst &zextInst) {
  LOG_DEBUG("SCCP visiting zext instruction");
  auto kind = getLatticeValue(zextInst.source().get());
  auto prev = getLatticeValue(&zextInst);
  if (kind != prev) {
    latticeValues_[&zextInst] = kind;
    // add users to worklist
    for (auto *user : zextInst.getUses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instructionWorklist_.push_back(inst);
      }
    }
  }
}

inline void SCCPVisitor::visit(ir::TruncInst &truncInst) {
  LOG_DEBUG("SCCP visiting trunc instruction");
  auto kind = getLatticeValue(truncInst.source().get());
  auto prev = getLatticeValue(&truncInst);
  if (kind != prev) {
    latticeValues_[&truncInst] = kind;
    // add users to worklist
    for (auto *user : truncInst.getUses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instructionWorklist_.push_back(inst);
      }
    }
  }
}

inline void SCCPVisitor::visit(ir::CallInst &callInst) {
  LOG_DEBUG("SCCP visiting call instruction");
  // overdef
  auto prev = getLatticeValue(&callInst);
  if (prev != LatticeValueKind::OVERDEF) {
    latticeValues_[&callInst] = LatticeValueKind::OVERDEF;
    // add users to worklist
    for (auto *user : callInst.getUses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instructionWorklist_.push_back(inst);
      }
    }
  }
}

inline void SCCPVisitor::visit(ir::PhiInst &phiInst) {
  LOG_DEBUG("SCCP visiting phi instruction");
}

inline void SCCPVisitor::visit(ir::SelectInst &selectInst) {
  LOG_DEBUG("SCCP visiting select instruction");

  auto prev = getLatticeValue(&selectInst);
  auto cond = getLatticeValue(selectInst.cond().get());
  if (cond == LatticeValueKind::CONSTANT) {
    auto trueKind = getLatticeValue(selectInst.ifTrue().get());
    auto falseKind = getLatticeValue(selectInst.ifFalse().get());
    LatticeValueKind resultKind;

    resultKind =
        dynamic_cast<ir::ConstantInt *>(selectInst.cond().get())->value() != 0
            ? trueKind
            : falseKind;

  } else if (cond == LatticeValueKind::UNDEF) {
    // result is UNDEF
    latticeValues_[&selectInst] = LatticeValueKind::UNDEF;
  } else {
    // cond == overdef, merge
    auto trueKind = getLatticeValue(selectInst.ifTrue().get());
    auto falseKind = getLatticeValue(selectInst.ifFalse().get());
    auto [resultKind, constValue] =
        mergePHIValues(trueKind, selectInst.ifTrue().get(), falseKind,
                       selectInst.ifFalse().get());
    latticeValues_[&selectInst] = resultKind;
    if (resultKind == LatticeValueKind::CONSTANT) {
      constantValues_[&selectInst] = constValue;
    }
  }

  if (latticeValues_[&selectInst] != prev) {
    // add users to worklist
    for (auto *user : selectInst.getUses()) {
      if (auto *inst = dynamic_cast<ir::Instruction *>(user)) {
        instructionWorklist_.push_back(inst);
      }
    }
  }
}

} // namespace rc::opt
