#include "backend/passes/regalloc.hpp"

namespace rc::backend {

void
RegAlloc::allocate(const std::vector<std::unique_ptr<AsmFunction>> &functions) {
  for (const auto &function : functions) {
    if (!function) {
      continue;
    }

    LOG_DEBUG("[RegAlloc] Begin function: " + function->name);
    size_t next_virtual_reg_id = max_virtual_register_id(*function) + 1;
    constexpr size_t k_max_rewrite_iterations = 32;
    const char *strategy_env = std::getenv("RC_REGALLOC");
    const bool use_linear_scan =
        strategy_env && std::string(strategy_env) == "linear";

    bool finished = false;
    for (size_t iteration = 0; iteration < k_max_rewrite_iterations; ++iteration) {
      clear_spill_flags(*function);

      auto intervals = build_intervals(*function);
      if (intervals.empty()) {
        finished = true;
        break;
      }

      if (use_linear_scan) {
        linear_scan(*function, intervals);
      } else {
        graph_color(*function, intervals);
      }

      bool has_spills = false;
      for (const auto &interval : intervals) {
        if (interval.spilled) {
          has_spills = true;
          break;
        }
      }

      if (!has_spills) {
        assign_physical_registers(*function, intervals);
        finished = true;
        break;
      }

      if (!rewrite_spills(*function, intervals, next_virtual_reg_id)) {
        throw std::runtime_error("RegAlloc: spill rewrite failed to make "
                                 "progress for function " +
                                 function->name);
      }
    }

    if (!finished) {
      throw std::runtime_error("RegAlloc: failed to allocate registers for " +
                               function->name);
    }

    function->stack_size = align_to(function->stack_size, 16);
    LOG_DEBUG("[RegAlloc] End function: " + function->name);
  }
}

size_t RegAlloc::align_to(size_t value, size_t align) const {
  if (align <= 1) {
    return value;
  }
  auto rem = value % align;
  return rem == 0 ? value : (value + (align - rem));
}

size_t
RegAlloc::max_virtual_register_id(const AsmFunction &function) const {
  size_t max_id = 0;
  for (const auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      size_t id = 0;
      if (is_virtual_register_operand(inst->get_dst(), &id)) {
        max_id = std::max(max_id, id);
      }
      for (const auto &use : inst->get_uses()) {
        if (is_virtual_register_operand(use, &id)) {
          max_id = std::max(max_id, id);
        }
      }
    }
  }
  return max_id;
}

std::vector<std::vector<size_t>>
RegAlloc::compute_successors(const AsmFunction &function) const {
  std::unordered_map<std::string, size_t> block_index_by_name;
  for (size_t i = 0; i < function.blocks.size(); ++i) {
    if (function.blocks[i]) {
      block_index_by_name[function.blocks[i]->name] = i;
    }
  }

  std::vector<std::vector<size_t>> successors(function.blocks.size());
  for (size_t i = 0; i < function.blocks.size(); ++i) {
    const auto &block = function.blocks[i];
    if (!block || block->instructions.empty()) {
      if (i + 1 < function.blocks.size()) {
        successors[i].push_back(i + 1);
      }
      continue;
    }

    const auto &terminator = block->instructions.back();
    if (!terminator) {
      continue;
    }

    const auto opcode = terminator->get_opcode();
    const auto &uses = terminator->get_uses();

    if (opcode == InstOpcode::J) {
      size_t label_pos = block_label_index(uses, opcode);
      if (label_pos < uses.size()) {
        auto label = std::dynamic_pointer_cast<Symbol>(uses[label_pos]);
        auto it =
            label ? block_index_by_name.find(label->name) : block_index_by_name.end();
        if (it != block_index_by_name.end()) {
          successors[i].push_back(it->second);
        }
      }
      continue;
    }

    if (opcode == InstOpcode::BNEZ || opcode == InstOpcode::BEQZ ||
        opcode == InstOpcode::BEQ || opcode == InstOpcode::BNE ||
        opcode == InstOpcode::BLT || opcode == InstOpcode::BGE ||
        opcode == InstOpcode::BLTU || opcode == InstOpcode::BGEU) {
      size_t first_label = block_label_index(uses, opcode);
      for (size_t pos = first_label; pos < uses.size(); ++pos) {
        auto label = std::dynamic_pointer_cast<Symbol>(uses[pos]);
        auto it =
            label ? block_index_by_name.find(label->name) : block_index_by_name.end();
        if (it != block_index_by_name.end()) {
          successors[i].push_back(it->second);
        }
      }
      continue;
    }

    if (!is_terminator(*terminator) && i + 1 < function.blocks.size()) {
      successors[i].push_back(i + 1);
    }
  }

  return successors;
}

std::unordered_map<int, RegAlloc::FixedRegisterSpan>
RegAlloc::compute_fixed_register_spans(const AsmFunction &function) const {
  std::unordered_map<int, FixedRegisterSpan> spans;
  size_t position = 0;

  for (const auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      auto record = [&](const std::shared_ptr<AsmOperand> &operand) {
        int reg_id = -1;
        if (!is_physical_register_operand(operand, &reg_id)) {
          return;
        }

        auto &span = spans[reg_id];
        if (!span.valid) {
          span.start = position;
          span.end = position;
          span.valid = true;
          return;
        }

        span.start = std::min(span.start, position);
        span.end = std::max(span.end, position);
      };

      record(inst->get_dst());
      for (const auto &use : inst->get_uses()) {
        record(use);
      }
      ++position;
    }
  }

  return spans;
}

std::vector<RegAlloc::LiveInterval>
RegAlloc::build_intervals(const AsmFunction &function) const {
  std::vector<BlockLiveness> blocks(function.blocks.size());
  size_t next_position = 0;

  for (size_t block_index = 0; block_index < function.blocks.size();
       ++block_index) {
    const auto &block = function.blocks[block_index];
    auto &info = blocks[block_index];
    info.start_pos = next_position;

    if (!block || block->instructions.empty()) {
      info.end_pos = next_position;
      continue;
    }

    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      size_t reg_id = 0;
      for (const auto &use : inst->get_uses()) {
        if (!is_virtual_register_operand(use, &reg_id)) {
          continue;
        }
        if (info.def.find(reg_id) == info.def.end()) {
          info.use.insert(reg_id);
        }
      }

      if (is_virtual_register_operand(inst->get_dst(), &reg_id)) {
        info.def.insert(reg_id);
      }

      ++next_position;
    }

    info.end_pos =
        next_position == info.start_pos ? info.start_pos : next_position - 1;
  }

  const auto successors = compute_successors(function);
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = function.blocks.size(); i-- > 0;) {
      std::unordered_set<size_t> new_live_out;
      for (size_t succ : successors[i]) {
        new_live_out.insert(blocks[succ].live_in.begin(),
                          blocks[succ].live_in.end());
      }

      std::unordered_set<size_t> new_live_in = blocks[i].use;
      for (size_t reg_id : new_live_out) {
        if (blocks[i].def.find(reg_id) == blocks[i].def.end()) {
          new_live_in.insert(reg_id);
        }
      }

      if (new_live_out != blocks[i].live_out || new_live_in != blocks[i].live_in) {
        blocks[i].live_out = std::move(new_live_out);
        blocks[i].live_in = std::move(new_live_in);
        changed = true;
      }
    }
  }

  std::unordered_map<size_t, LiveInterval> interval_map;
  std::vector<size_t> call_positions;
  size_t position = 0;

  for (size_t block_index = 0; block_index < function.blocks.size();
       ++block_index) {
    const auto &block = function.blocks[block_index];
    const auto &info = blocks[block_index];

    if (!block || block->instructions.empty()) {
      continue;
    }

    for (size_t reg_id : info.live_in) {
      extend_interval(interval_map, reg_id, info.start_pos, info.end_pos);
    }
    for (size_t reg_id : info.live_out) {
      extend_interval(interval_map, reg_id, info.start_pos, info.end_pos);
    }

    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      if (is_call(*inst)) {
        call_positions.push_back(position);
      }

      size_t reg_id = 0;
      for (const auto &use : inst->get_uses()) {
        if (is_virtual_register_operand(use, &reg_id)) {
          extend_interval(interval_map, reg_id, position, position);
        }
      }
      if (is_virtual_register_operand(inst->get_dst(), &reg_id)) {
        extend_interval(interval_map, reg_id, position, position);
      }

      ++position;
    }
  }

  std::vector<LiveInterval> intervals;
  intervals.reserve(interval_map.size());
  for (auto &[_, interval] : interval_map) {
    for (size_t call_pos : call_positions) {
      if (interval.start < call_pos && call_pos < interval.end) {
        interval.crosses_call = true;
        break;
      }
    }
    intervals.push_back(interval);
  }

  std::sort(intervals.begin(), intervals.end(),
            [](const LiveInterval &lhs, const LiveInterval &rhs) {
              if (lhs.start != rhs.start) {
                return lhs.start < rhs.start;
              }
              if (lhs.end != rhs.end) {
                return lhs.end < rhs.end;
              }
              return lhs.vreg_id < rhs.vreg_id;
            });

  return intervals;
}

void RegAlloc::linear_scan(AsmFunction &function,
                                 std::vector<LiveInterval> &intervals) const {
  std::unordered_set<size_t> spilled_ids;
  std::vector<LiveInterval *> active;
  const auto fixed_reg_spans = compute_fixed_register_spans(function);
  auto sort_active = [&]() {
    std::sort(active.begin(), active.end(),
              [](const LiveInterval *lhs, const LiveInterval *rhs) {
                if (lhs->end != rhs->end) {
                  return lhs->end < rhs->end;
                }
                return lhs->vreg_id < rhs->vreg_id;
              });
  };

  auto allocate_spill = [&](LiveInterval &interval) {
    interval.spilled = true;
    interval.assigned_phys_reg = -1;
    interval.spill_slot = create_spill_slot(function);
    spilled_ids.insert(interval.vreg_id);
  };

  for (auto &interval : intervals) {
    active.erase(std::remove_if(active.begin(), active.end(),
                                [&](const LiveInterval *other) {
                                  return other->end < interval.start;
                                }),
                 active.end());
    sort_active();

    const auto &candidates = candidate_registers(interval.crosses_call);
    std::vector<int> available;
    available.reserve(candidates.size());

    for (int phys_reg : candidates) {
      bool occupied = false;
      for (const auto *live : active) {
        if (live->assigned_phys_reg == phys_reg) {
          occupied = true;
          break;
        }
      }
      if (occupied) {
        continue;
      }

      auto fixed_it = fixed_reg_spans.find(phys_reg);
      if (fixed_it != fixed_reg_spans.end() &&
          overlaps_fixed_register(fixed_it->second, interval.start, interval.end)) {
        continue;
      }

      available.push_back(phys_reg);
    }

    if (available.empty()) {
      LiveInterval *spill = nullptr;
      auto spill_it = active.end();
      for (auto it = active.begin(); it != active.end(); ++it) {
        if (std::find(candidates.begin(), candidates.end(),
                      (*it)->assigned_phys_reg) == candidates.end()) {
          continue;
        }

        if (!spill || spill->end < (*it)->end ||
            (spill->end == (*it)->end && spill->vreg_id < (*it)->vreg_id)) {
          spill = *it;
          spill_it = it;
        }
      }

      if (!spill || spill_it == active.end()) {
        allocate_spill(interval);
        continue;
      }

      if (spill->end > interval.end) {
        interval.assigned_phys_reg = spill->assigned_phys_reg;
        spill->assigned_phys_reg = -1;
        spill->spill_slot = create_spill_slot(function);
        spill->spilled = true;
        spilled_ids.insert(spill->vreg_id);
        *spill_it = &interval;
        sort_active();
      } else {
        allocate_spill(interval);
      }
      continue;
    }

    interval.assigned_phys_reg = available.front();
    active.push_back(&interval);
    sort_active();
  }

  mark_spilled_registers(function, spilled_ids);
}

void RegAlloc::graph_color(AsmFunction &function,
                                 std::vector<LiveInterval> &intervals) const {
  std::unordered_set<size_t> spilled_ids;
  const auto fixed_reg_spans = compute_fixed_register_spans(function);
  const size_t n = intervals.size();
  const size_t original_stack_size = function.stack_size;
  constexpr size_t k_graph_color_interval_limit = 1024;
  if (n > k_graph_color_interval_limit) {
    LOG_DEBUG("[RegAlloc] Falling back to linear scan for large function: " +
              function.name);
    linear_scan(function, intervals);
    return;
  }

  std::unordered_map<size_t, size_t> index_by_vreg;
  index_by_vreg.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    index_by_vreg[intervals[i].vreg_id] = i;
  }

  std::vector<std::vector<size_t>> graph(n);
  std::vector<std::unordered_set<size_t>> edge_sets(n);

  auto add_edge = [&](size_t lhs_reg, size_t rhs_reg) {
    if (lhs_reg == rhs_reg) {
      return;
    }
    auto lhs_it = index_by_vreg.find(lhs_reg);
    auto rhs_it = index_by_vreg.find(rhs_reg);
    if (lhs_it == index_by_vreg.end() || rhs_it == index_by_vreg.end()) {
      return;
    }
    size_t lhs = lhs_it->second;
    size_t rhs = rhs_it->second;
    edge_sets[lhs].insert(rhs);
    edge_sets[rhs].insert(lhs);
  };

  std::vector<BlockLiveness> blocks(function.blocks.size());
  for (size_t block_index = 0; block_index < function.blocks.size();
       ++block_index) {
    const auto &block = function.blocks[block_index];
    if (!block) {
      continue;
    }

    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      size_t reg_id = 0;
      for (const auto &use : inst->get_uses()) {
        if (!is_virtual_register_operand(use, &reg_id)) {
          continue;
        }
        if (blocks[block_index].def.find(reg_id) ==
            blocks[block_index].def.end()) {
          blocks[block_index].use.insert(reg_id);
        }
      }

      if (is_virtual_register_operand(inst->get_dst(), &reg_id)) {
        blocks[block_index].def.insert(reg_id);
      }
    }
  }

  const auto successors = compute_successors(function);
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = function.blocks.size(); i-- > 0;) {
      std::unordered_set<size_t> new_live_out;
      for (size_t succ : successors[i]) {
        new_live_out.insert(blocks[succ].live_in.begin(),
                          blocks[succ].live_in.end());
      }

      std::unordered_set<size_t> new_live_in = blocks[i].use;
      for (size_t reg_id : new_live_out) {
        if (blocks[i].def.find(reg_id) == blocks[i].def.end()) {
          new_live_in.insert(reg_id);
        }
      }

      if (new_live_out != blocks[i].live_out || new_live_in != blocks[i].live_in) {
        blocks[i].live_out = std::move(new_live_out);
        blocks[i].live_in = std::move(new_live_in);
        changed = true;
      }
    }
  }

  for (size_t block_index = 0; block_index < function.blocks.size();
       ++block_index) {
    const auto &block = function.blocks[block_index];
    if (!block) {
      continue;
    }

    auto live = blocks[block_index].live_out;
    for (auto inst_it = block->instructions.rbegin();
         inst_it != block->instructions.rend(); ++inst_it) {
      const auto &inst = *inst_it;
      if (!inst) {
        continue;
      }

      std::vector<size_t> use_regs;
      size_t reg_id = 0;
      for (const auto &use : inst->get_uses()) {
        if (is_virtual_register_operand(use, &reg_id)) {
          use_regs.push_back(reg_id);
        }
      }
      for (size_t i = 0; i < use_regs.size(); ++i) {
        for (size_t j = i + 1; j < use_regs.size(); ++j) {
          add_edge(use_regs[i], use_regs[j]);
        }
      }

      size_t dst_id = 0;
      if (is_virtual_register_operand(inst->get_dst(), &dst_id)) {
        for (size_t live_reg : live) {
          add_edge(dst_id, live_reg);
        }
        live.erase(dst_id);
      }

      for (size_t use_reg : use_regs) {
        live.insert(use_reg);
      }
    }
  }

  for (size_t i = 0; i < n; ++i) {
    graph[i].assign(edge_sets[i].begin(), edge_sets[i].end());
  }

  auto available_colors = [&](const LiveInterval &interval) {
    std::vector<int> colors;
    const auto &candidates = candidate_registers(interval.crosses_call);
    colors.reserve(candidates.size());
    for (int phys_reg : candidates) {
      auto fixed_it = fixed_reg_spans.find(phys_reg);
      if (fixed_it != fixed_reg_spans.end() &&
          overlaps_fixed_register(fixed_it->second, interval.start,
                                interval.end)) {
        continue;
      }
      colors.push_back(phys_reg);
    }
    return colors;
  };

  std::vector<std::vector<int>> colors(n);
  std::vector<size_t> order(n);
  for (size_t i = 0; i < n; ++i) {
    colors[i] = available_colors(intervals[i]);
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [&](size_t lhs, size_t rhs) {
    if (colors[lhs].empty() != colors[rhs].empty()) {
      return !colors[lhs].empty();
    }
    if (graph[lhs].size() != graph[rhs].size()) {
      return graph[lhs].size() > graph[rhs].size();
    }
    const auto lhs_len = intervals[lhs].end - intervals[lhs].start;
    const auto rhs_len = intervals[rhs].end - intervals[rhs].start;
    if (lhs_len != rhs_len) {
      return lhs_len > rhs_len;
    }
    return intervals[lhs].vreg_id < intervals[rhs].vreg_id;
  });

  std::vector<int> assigned(n, -1);
  for (size_t index : order) {
    std::unordered_set<int> unavailable;
    for (size_t neighbor : graph[index]) {
      if (assigned[neighbor] >= 0) {
        unavailable.insert(assigned[neighbor]);
      }
    }

    int color = -1;
    for (int candidate : colors[index]) {
      if (unavailable.find(candidate) == unavailable.end()) {
        color = candidate;
        break;
      }
    }

    if (color < 0) {
      intervals[index].spilled = true;
      intervals[index].assigned_phys_reg = -1;
      intervals[index].spill_slot = create_spill_slot(function);
      spilled_ids.insert(intervals[index].vreg_id);
      continue;
    }

    assigned[index] = color;
    intervals[index].assigned_phys_reg = color;
    intervals[index].spilled = false;
    intervals[index].spill_slot = nullptr;
  }

  if (!spilled_ids.empty()) {
    function.stack_size = original_stack_size;
    for (auto &interval : intervals) {
      interval.assigned_phys_reg = -1;
      interval.spilled = false;
      interval.spill_slot = nullptr;
    }
    linear_scan(function, intervals);
    return;
  }

  mark_spilled_registers(function, spilled_ids);
}

bool RegAlloc::rewrite_spills(AsmFunction &function,
                                    const std::vector<LiveInterval> &intervals,
                                    size_t &next_virtual_reg_id) const {
  std::unordered_map<size_t, std::shared_ptr<StackSlot>> spilled_slots;
  for (const auto &interval : intervals) {
    if (interval.spilled && interval.spill_slot) {
      spilled_slots[interval.vreg_id] = interval.spill_slot;
    }
  }

  if (spilled_slots.empty()) {
    return false;
  }

  bool changed = false;
  for (auto &block : function.blocks) {
    if (!block) {
      continue;
    }

    std::vector<std::unique_ptr<AsmInst>> rewritten;
    rewritten.reserve(block->instructions.size() * 3);

    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      std::vector<std::shared_ptr<AsmOperand>> new_uses = inst->get_uses();
      std::vector<std::unique_ptr<AsmInst>> before;
      std::vector<std::unique_ptr<AsmInst>> after;
      std::unordered_map<size_t, std::shared_ptr<Register>> loaded_temps;

      auto load_spilled_use = [&](size_t reg_id,
                                const std::shared_ptr<StackSlot> &slot)
          -> std::shared_ptr<Register> {
        auto found = loaded_temps.find(reg_id);
        if (found != loaded_temps.end()) {
          return found->second;
        }

        auto temp = create_virtual_register(next_virtual_reg_id++);
        before.push_back(std::make_unique<AsmInst>(
            InstOpcode::LW, temp,
            std::vector<std::shared_ptr<AsmOperand>>{slot}));
        loaded_temps.emplace(reg_id, temp);
        return temp;
      };

      for (size_t i = 0; i < new_uses.size(); ++i) {
        size_t reg_id = 0;
        if (!is_virtual_register_operand(new_uses[i], &reg_id)) {
          continue;
        }

        auto spill_it = spilled_slots.find(reg_id);
        if (spill_it == spilled_slots.end()) {
          continue;
        }

        new_uses[i] = load_spilled_use(reg_id, spill_it->second);
        changed = true;
      }

      auto new_dst = inst->get_dst();
      size_t dst_id = 0;
      if (is_virtual_register_operand(new_dst, &dst_id)) {
        auto spill_it = spilled_slots.find(dst_id);
        if (spill_it != spilled_slots.end()) {
          auto temp_dst = create_virtual_register(next_virtual_reg_id++);
          new_dst = temp_dst;
          after.push_back(std::make_unique<AsmInst>(
              InstOpcode::SW, std::vector<std::shared_ptr<AsmOperand>>{
                                  temp_dst, spill_it->second}));
          changed = true;
        }
      }

      for (auto &pre : before) {
        rewritten.push_back(std::move(pre));
      }
      rewritten.push_back(clone_inst(*inst, new_dst, new_uses));
      for (auto &post : after) {
        rewritten.push_back(std::move(post));
      }
    }

    block->instructions = std::move(rewritten);
  }

  return changed;
}

void RegAlloc::assign_physical_registers(
    AsmFunction &function, const std::vector<LiveInterval> &intervals) const {
  std::unordered_map<size_t, int> assignment;
  for (const auto &interval : intervals) {
    if (interval.spilled || interval.assigned_phys_reg < 0) {
      throw std::runtime_error("RegAlloc: virtual register left without a "
                               "physical assignment");
    }
    assignment[interval.vreg_id] = interval.assigned_phys_reg;
  }

  std::unordered_map<int, std::shared_ptr<Register>> physical_regs;
  auto materialize = [&](const std::shared_ptr<AsmOperand> &operand)
      -> std::shared_ptr<AsmOperand> {
    size_t reg_id = 0;
    if (!is_virtual_register_operand(operand, &reg_id)) {
      return operand;
    }

    auto it = assignment.find(reg_id);
    if (it == assignment.end()) {
      throw std::runtime_error("RegAlloc: missing assignment for v" +
                               std::to_string(reg_id));
    }

    auto cache_it = physical_regs.find(it->second);
    if (cache_it != physical_regs.end()) {
      return cache_it->second;
    }

    auto reg = create_physical_register(static_cast<size_t>(it->second));
    physical_regs.emplace(it->second, reg);
    return reg;
  };

  for (auto &block : function.blocks) {
    if (!block) {
      continue;
    }

    std::vector<std::unique_ptr<AsmInst>> rewritten;
    rewritten.reserve(block->instructions.size());
    for (const auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      std::vector<std::shared_ptr<AsmOperand>> uses;
      uses.reserve(inst->get_uses().size());
      for (const auto &use : inst->get_uses()) {
        uses.push_back(materialize(use));
      }
      rewritten.push_back(clone_inst(*inst, materialize(inst->get_dst()), uses));
    }
    block->instructions = std::move(rewritten);
  }
}

std::shared_ptr<Register>
RegAlloc::create_virtual_register(size_t id) const {
  auto reg = std::make_shared<Register>();
  reg->id = id;
  reg->is_virtual = true;
  reg->spilled = false;
  return reg;
}

std::shared_ptr<Register>
RegAlloc::create_physical_register(size_t id) const {
  auto reg = std::make_shared<Register>();
  reg->id = id;
  reg->is_virtual = false;
  reg->spilled = false;
  return reg;
}

std::shared_ptr<StackSlot>
RegAlloc::create_spill_slot(AsmFunction &function) const {
  size_t offset = align_to(function.stack_size, k_spill_slot_size);
  auto slot = std::make_shared<StackSlot>(offset, k_spill_slot_size);
  function.stack_size = offset + k_spill_slot_size;
  return slot;
}

void RegAlloc::clear_spill_flags(AsmFunction &function) const {
  for (auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }

      if (auto dst = std::dynamic_pointer_cast<Register>(inst->get_dst());
          dst && dst->is_virtual) {
        dst->spilled = false;
      }

      for (const auto &use : inst->get_uses()) {
        auto reg = std::dynamic_pointer_cast<Register>(use);
        if (reg && reg->is_virtual) {
          reg->spilled = false;
        }
      }
    }
  }
}

void RegAlloc::mark_spilled_registers(
    AsmFunction &function, const std::unordered_set<size_t> &spilled) const {
  if (spilled.empty()) {
    return;
  }

  auto mark = [&](const std::shared_ptr<AsmOperand> &operand) {
    auto reg = std::dynamic_pointer_cast<Register>(operand);
    if (reg && reg->is_virtual && spilled.find(reg->id) != spilled.end()) {
      reg->spilled = true;
    }
  };

  for (auto &block : function.blocks) {
    if (!block) {
      continue;
    }
    for (auto &inst : block->instructions) {
      if (!inst) {
        continue;
      }
      mark(inst->get_dst());
      for (const auto &use : inst->get_uses()) {
        mark(use);
      }
    }
  }
}

bool
RegAlloc::is_virtual_register_operand(const std::shared_ptr<AsmOperand> &operand,
                                   size_t *id) const {
  auto reg = std::dynamic_pointer_cast<Register>(operand);
  if (!reg || !reg->is_virtual) {
    return false;
  }
  if (id) {
    *id = reg->id;
  }
  return true;
}

bool
RegAlloc::is_physical_register_operand(const std::shared_ptr<AsmOperand> &operand,
                                    int *id) const {
  auto reg = std::dynamic_pointer_cast<Register>(operand);
  if (!reg || reg->is_virtual) {
    return false;
  }
  if (id) {
    *id = static_cast<int>(reg->id);
  }
  return true;
}

bool RegAlloc::is_call(const AsmInst &inst) const {
  return inst.get_opcode() == InstOpcode::CALL;
}

bool RegAlloc::is_terminator(const AsmInst &inst) const {
  switch (inst.get_opcode()) {
  case InstOpcode::RET:
  case InstOpcode::J:
  case InstOpcode::BEQZ:
  case InstOpcode::BNEZ:
  case InstOpcode::BEQ:
  case InstOpcode::BNE:
  case InstOpcode::BLT:
  case InstOpcode::BGE:
  case InstOpcode::BLTU:
  case InstOpcode::BGEU:
    return true;
  default:
    return false;
  }
}

const std::vector<int> &
RegAlloc::candidate_registers(bool crosses_call) const {
  static const std::vector<int> k_cross_call_candidates(k_callee_saved_regs.begin(),
                                                     k_callee_saved_regs.end());
  static const std::vector<int> k_general_candidates = []() {
    std::vector<int> regs;
    regs.reserve(k_caller_saved_regs.size() + k_callee_saved_regs.size());
    regs.insert(regs.end(), k_caller_saved_regs.begin(), k_caller_saved_regs.end());
    regs.insert(regs.end(), k_callee_saved_regs.begin(), k_callee_saved_regs.end());
    return regs;
  }();

  return crosses_call ? k_cross_call_candidates : k_general_candidates;
}

bool RegAlloc::overlaps_fixed_register(const FixedRegisterSpan &span,
                                            size_t start, size_t end) const {
  if (!span.valid) {
    return false;
  }
  return !(end < span.start || span.end < start);
}

size_t
RegAlloc::block_label_index(const std::vector<std::shared_ptr<AsmOperand>> &uses,
                          InstOpcode opcode) const {
  (void)uses;
  switch (opcode) {
  case InstOpcode::BNEZ:
  case InstOpcode::BEQZ:
    return 1;
  case InstOpcode::BEQ:
  case InstOpcode::BNE:
  case InstOpcode::BLT:
  case InstOpcode::BGE:
  case InstOpcode::BLTU:
  case InstOpcode::BGEU:
    return 2;
  default:
    return 0;
  }
}

void
RegAlloc::extend_interval(std::unordered_map<size_t, LiveInterval> &intervals,
                         size_t vreg_id, size_t start, size_t end) const {
  auto &interval = intervals[vreg_id];
  if (interval.vreg_id == 0) {
    interval.vreg_id = vreg_id;
    interval.start = start;
    interval.end = end;
    return;
  }

  interval.start = std::min(interval.start, start);
  interval.end = std::max(interval.end, end);
}

std::unique_ptr<AsmInst> RegAlloc::clone_inst(
    const AsmInst &inst, const std::shared_ptr<AsmOperand> &dst,
    const std::vector<std::shared_ptr<AsmOperand>> &uses) const {
  if (dst) {
    return std::make_unique<AsmInst>(inst.get_opcode(), dst, uses);
  }
  return std::make_unique<AsmInst>(inst.get_opcode(), uses);
}

} // namespace rc::backend
