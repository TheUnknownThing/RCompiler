#include "opt/utils/ir_utils.hpp"

namespace rc::opt::utils {

void replace_all_uses_with(ir::Value &from, ir::Value *to) {
  if (!to || &from == to) {
    return;
  }

  std::vector<ir::Instruction *> users;
  users.reserve(from.get_uses().size());
  for (auto *u : from.get_uses()) {
    users.push_back(u);
  }

  for (auto *user : users) {
    if (!user) {
      continue;
    }
    user->replace_operand(&from, to);
  }
}

void replace_all_uses_with(ir::Value *from, ir::Value *to) {
  if (!from) {
    return;
  }
  replace_all_uses_with(*from, to);
}

std::shared_ptr<ir::Instruction>
find_shared_instruction(ir::BasicBlock &bb, ir::Instruction *inst) {
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

bool erase_instruction(ir::BasicBlock &bb, ir::Instruction *inst) {
  auto sp = find_shared_instruction(bb, inst);
  if (!sp) {
    return false;
  }
  bb.erase_instruction(sp);
  return true;
}

std::shared_ptr<ir::ConstantInt> as_const_int(ir::Value *v) {
  if (!v) {
    return nullptr;
  }
  if (auto *ci = dynamic_cast<ir::ConstantInt *>(v)) {
    return std::dynamic_pointer_cast<ir::ConstantInt>(ci->shared_from_this());
  }
  return nullptr;
}

bool is_const_int(ir::Value *v, std::uint64_t val) {
  auto ci = as_const_int(v);
  return ci && ci->value() == val;
}

std::optional<unsigned> int_bits(const ir::TypePtr &ty) {
  auto it = std::dynamic_pointer_cast<const ir::IntegerType>(ty);
  if (!it) {
    return std::nullopt;
  }
  return it->bits();
}

} // namespace rc::opt::utils
