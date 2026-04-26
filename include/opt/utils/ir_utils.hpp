#pragma once

#include "ir/instructions/top_level.hpp"
#include "ir/instructions/type.hpp"

#include <memory>
#include <vector>
#include <optional>

namespace rc::opt::utils {

void replace_all_uses_with(ir::Value &from, ir::Value *to);

void replace_all_uses_with(ir::Value *from, ir::Value *to);

std::shared_ptr<ir::Instruction>
find_shared_instruction(ir::BasicBlock &bb, ir::Instruction *inst);

bool erase_instruction(ir::BasicBlock &bb, ir::Instruction *inst);

std::shared_ptr<ir::ConstantInt> as_const_int(ir::Value *v);

bool is_const_int(ir::Value *v, std::uint64_t val);

std::optional<unsigned> int_bits(const ir::TypePtr &ty);

} // namespace rc::opt::utils
