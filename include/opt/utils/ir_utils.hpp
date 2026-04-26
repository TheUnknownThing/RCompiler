#pragma once

#include "ir/instructions/topLevel.hpp"
#include "ir/instructions/type.hpp"

#include <memory>
#include <vector>
#include <optional>

namespace rc::opt::utils {

void replaceAllUsesWith(ir::Value &from, ir::Value *to);

void replaceAllUsesWith(ir::Value *from, ir::Value *to);

std::shared_ptr<ir::Instruction>
findSharedInstruction(ir::BasicBlock &bb, ir::Instruction *inst);

bool eraseInstruction(ir::BasicBlock &bb, ir::Instruction *inst);

std::shared_ptr<ir::ConstantInt> asConstInt(ir::Value *v);

bool isConstInt(ir::Value *v, std::uint64_t val);

std::optional<unsigned> intBits(const ir::TypePtr &ty);

} // namespace rc::opt::utils
