#pragma once

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/top_level.hpp"

namespace rc::opt::utils {

namespace detail {

std::unordered_map<const ir::BasicBlock *, std::size_t>
index_blocks(const ir::Function &fn);

std::string node_id(std::size_t index);

std::string node_label(const ir::BasicBlock &bb, std::size_t index);

std::vector<const ir::BasicBlock *> successors(const ir::BasicBlock &bb);

std::vector<ir::BasicBlock *> successors(ir::BasicBlock &bb);

std::vector<const ir::BasicBlock *>
dedup_and_sort_by_index(std::vector<const ir::BasicBlock *> v,
                    const std::unordered_map<const ir::BasicBlock *, std::size_t>
                        &index);

std::string escape_dot_label(std::string s);

} // namespace detail

std::string cfg_to_string(const ir::Function &fn);

std::string cfg_to_dot(const ir::Function &fn);

} // namespace rc::opt::utils
