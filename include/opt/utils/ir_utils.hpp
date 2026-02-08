#pragma once

#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include <memory>
#include <vector>
#include <optional>

namespace rc::opt::utils {

inline void replaceAllUsesWith(ir::Value &from, ir::Value *to) {
  if (!to || &from == to) {
    return;
  }

  std::vector<ir::Instruction *> users;
  users.reserve(from.getUses().size());
  for (auto *u : from.getUses()) {
    users.push_back(u);
  }

  for (auto *user : users) {
    if (!user) {
      continue;
    }
    user->replaceOperand(&from, to);
  }
}

inline void replaceAllUsesWith(ir::Value *from, ir::Value *to) {
  if (!from) {
    return;
  }
  replaceAllUsesWith(*from, to);
}

inline std::shared_ptr<ir::Instruction>
findSharedInstruction(ir::BasicBlock &bb, ir::Instruction *inst) {
  if (!inst) {
    return nullptr;
  }
  for (auto &sp : bb.instructions()) {
    if (sp.get() == inst) {
      return sp;
    }
  }
  return nullptr;
}

inline bool eraseInstruction(ir::BasicBlock &bb, ir::Instruction *inst) {
  auto sp = findSharedInstruction(bb, inst);
  if (!sp) {
    return false;
  }
  bb.eraseInstruction(sp);
  return true;
}

inline std::shared_ptr<ir::ConstantInt> asConstInt(ir::Value *v) {
  if (!v) {
    return nullptr;
  }
  if (auto *ci = dynamic_cast<ir::ConstantInt *>(v)) {
    return std::dynamic_pointer_cast<ir::ConstantInt>(ci->shared_from_this());
  }
  return nullptr;
}

inline bool isConstInt(ir::Value *v, std::uint64_t val) {
  auto ci = asConstInt(v);
  return ci && ci->value() == val;
}

inline std::optional<unsigned> intBits(const ir::TypePtr &ty) {
  auto it = std::dynamic_pointer_cast<const ir::IntegerType>(ty);
  if (!it) {
    return std::nullopt;
  }
  return it->bits();
}

} // namespace rc::opt::utils
