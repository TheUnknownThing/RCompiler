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

inline std::unordered_map<const ir::BasicBlock *, std::size_t>
indexBlocks(const ir::Function &fn) {
  std::unordered_map<const ir::BasicBlock *, std::size_t> index;
  const auto &blocks = fn.blocks();
  index.reserve(blocks.size());
  for (std::size_t i = 0; i < blocks.size(); ++i) {
    index.emplace(blocks[i].get(), i);
  }
  return index;
}

inline std::string nodeId(std::size_t index) {
  return "bb" + std::to_string(index);
}

inline std::string nodeLabel(const ir::BasicBlock &bb, std::size_t index) {
  if (!bb.name().empty()) {
    return bb.name() + " (" + nodeId(index) + ")";
  }
  return nodeId(index);
}

inline std::vector<const ir::BasicBlock *> successors(const ir::BasicBlock &bb) {
  std::vector<const ir::BasicBlock *> outs;
  const auto &ins = bb.instructions();
  if (ins.empty()) {
    return outs;
  }

  for (const auto &inst : ins) {
    if (!inst) {
      continue;
    }
    if (auto *br = dynamic_cast<const ir::BranchInst *>(inst.get())) {
      if (br->dest()) {
        outs.push_back(br->dest());
      }
      if (br->isConditional() && br->altDest()) {
        outs.push_back(br->altDest());
      }
      break;
    }
    if (dynamic_cast<const ir::ReturnInst *>(inst.get()) ||
        dynamic_cast<const ir::UnreachableInst *>(inst.get())) {
      break;
    }
  }

  return outs;
}

inline std::vector<ir::BasicBlock *> successors(ir::BasicBlock &bb) {
  std::vector<ir::BasicBlock *> outs;
  auto &ins = bb.instructions();
  if (ins.empty()) {
    return outs;
  }

  for (const auto &inst : ins) {
    if (!inst) {
      continue;
    }
    if (auto *br = dynamic_cast<const ir::BranchInst *>(inst.get())) {
      if (br->dest()) {
        outs.push_back(br->dest());
      }
      if (br->isConditional() && br->altDest()) {
        outs.push_back(br->altDest());
      }
      break;
    }
    if (dynamic_cast<const ir::ReturnInst *>(inst.get()) ||
        dynamic_cast<const ir::UnreachableInst *>(inst.get())) {
      break;
    }
  }

  return outs;
}

inline std::vector<const ir::BasicBlock *>
dedupAndSortByIndex(std::vector<const ir::BasicBlock *> v,
                    const std::unordered_map<const ir::BasicBlock *, std::size_t>
                        &index) {
  std::unordered_set<const ir::BasicBlock *> seen;
  seen.reserve(v.size());

  std::vector<const ir::BasicBlock *> out;
  out.reserve(v.size());
  for (const auto *bb : v) {
    if (!bb) {
      continue;
    }
    if (seen.insert(bb).second) {
      out.push_back(bb);
    }
  }

  std::sort(out.begin(), out.end(), [&](const ir::BasicBlock *a,
                                        const ir::BasicBlock *b) {
    const auto ia = index.find(a);
    const auto ib = index.find(b);
    const auto va = ia == index.end() ? std::numeric_limits<std::size_t>::max()
                                      : ia->second;
    const auto vb = ib == index.end() ? std::numeric_limits<std::size_t>::max()
                                      : ib->second;
    return va < vb;
  });

  return out;
}

inline std::string escapeDotLabel(std::string s) {
  // Minimal escaping for DOT labels.
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    if (c == '\n') {
      out += "\\n";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

} // namespace detail

inline std::string cfgToString(const ir::Function &fn) {
  std::ostringstream oss;
  oss << "CFG for function '" << fn.name() << "'";

  const auto &blocks = fn.blocks();
  if (blocks.empty()) {
    oss << " (no blocks)";
    return oss.str();
  }

  const auto index = detail::indexBlocks(fn);
  oss << " (" << blocks.size() << " blocks)\n";

  for (std::size_t i = 0; i < blocks.size(); ++i) {
    const auto &bb = *blocks[i];
    oss << "- " << detail::nodeLabel(bb, i) << "\n";

    // Predecessors
    {
      std::vector<const ir::BasicBlock *> preds;
      preds.reserve(bb.predecessors().size());
      for (const auto *p : bb.predecessors()) {
        preds.push_back(p);
      }
      preds = detail::dedupAndSortByIndex(std::move(preds), index);

      oss << "  preds: ";
      if (preds.empty()) {
        oss << "(none)\n";
      } else {
        for (std::size_t k = 0; k < preds.size(); ++k) {
          const auto *p = preds[k];
          const auto it = index.find(p);
          const auto pi = it == index.end() ? std::numeric_limits<std::size_t>::max()
                                            : it->second;
          if (k) {
            oss << ", ";
          }
          if (it == index.end()) {
            oss << "<external>";
          } else {
            oss << detail::nodeLabel(*p, pi);
          }
        }
        oss << "\n";
      }
    }

    // Successors (computed from terminator)
    {
      auto succs = detail::successors(bb);
      succs = detail::dedupAndSortByIndex(std::move(succs), index);

      oss << "  succs: ";
      if (succs.empty()) {
        oss << "(none)\n";
      } else {
        for (std::size_t k = 0; k < succs.size(); ++k) {
          const auto *s = succs[k];
          const auto it = index.find(s);
          const auto si = it == index.end() ? std::numeric_limits<std::size_t>::max()
                                            : it->second;
          if (k) {
            oss << ", ";
          }
          if (it == index.end()) {
            oss << "<external>";
          } else {
            oss << detail::nodeLabel(*s, si);
          }
        }
        oss << "\n";
      }
    }
  }

  return oss.str();
}

inline std::string cfgToDot(const ir::Function &fn) {
  std::ostringstream oss;

  const auto &blocks = fn.blocks();
  const auto index = detail::indexBlocks(fn);

  oss << "digraph CFG {\n";
  oss << "  labelloc=\"t\";\n";
  oss << "  label=\"CFG: " << detail::escapeDotLabel(fn.name()) << "\";\n";
  oss << "  node [shape=box, fontname=\"monospace\"];\n";

  for (std::size_t i = 0; i < blocks.size(); ++i) {
    const auto &bb = *blocks[i];
    oss << "  " << detail::nodeId(i) << " [label=\""
        << detail::escapeDotLabel(detail::nodeLabel(bb, i)) << "\"];\n";
  }

  for (std::size_t i = 0; i < blocks.size(); ++i) {
    const auto &bb = *blocks[i];
    auto succs = detail::successors(bb);
    succs = detail::dedupAndSortByIndex(std::move(succs), index);

    for (const auto *s : succs) {
      const auto it = index.find(s);
      if (it == index.end()) {
        continue;
      }
      oss << "  " << detail::nodeId(i) << " -> " << detail::nodeId(it->second)
          << ";\n";
    }
  }

  oss << "}\n";
  return oss.str();
}

} // namespace rc::opt::utils
