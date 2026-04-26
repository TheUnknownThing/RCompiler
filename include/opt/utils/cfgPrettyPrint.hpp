#pragma once

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/topLevel.hpp"

namespace rc::opt::utils {

namespace detail {

std::unordered_map<const ir::BasicBlock *, std::size_t>
indexBlocks(const ir::Function &fn);

std::string nodeId(std::size_t index);

std::string nodeLabel(const ir::BasicBlock &bb, std::size_t index);

std::vector<const ir::BasicBlock *> successors(const ir::BasicBlock &bb);

std::vector<ir::BasicBlock *> successors(ir::BasicBlock &bb);

std::vector<const ir::BasicBlock *>
dedupAndSortByIndex(std::vector<const ir::BasicBlock *> v,
                    const std::unordered_map<const ir::BasicBlock *, std::size_t>
                        &index);

std::string escapeDotLabel(std::string s);

} // namespace detail

std::string cfgToString(const ir::Function &fn);

std::string cfgToDot(const ir::Function &fn);

} // namespace rc::opt::utils
