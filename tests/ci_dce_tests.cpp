#include <iostream>
#include <sstream>
#include <string>

#include "ir/instructions/binary.hpp"
#include "ir/instructions/control_flow.hpp"
#include "ir/instructions/memory.hpp"
#include "ir/instructions/top_level.hpp"
#include "opt/dce/dce.hpp"

namespace {
int failures = 0;

void record_failure(const std::string &message) {
  ++failures;
  std::cerr << message << "\n";
}

rc::ir::BranchInst *get_branch(rc::ir::BasicBlock &bb) {
  auto &ins = bb.instructions();
  for (auto &inst : ins) {
    if (auto *br = dynamic_cast<rc::ir::BranchInst *>(inst.get())) {
      return br;
    }
  }
  return nullptr;
}

rc::ir::UnreachableInst *get_unreachable(rc::ir::BasicBlock &bb) {
  auto &ins = bb.instructions();
  for (auto &inst : ins) {
    if (auto *u = dynamic_cast<rc::ir::UnreachableInst *>(inst.get())) {
      return u;
    }
  }
  return nullptr;
}

std::size_t count_binary_ops(rc::ir::BasicBlock &bb) {
  std::size_t count = 0;
  for (auto &inst : bb.instructions()) {
    if (dynamic_cast<rc::ir::BinaryOpInst *>(inst.get())) {
      ++count;
    }
  }
  return count;
}

void test_fold_true_branch() {
  rc::ir::Module m("m");
  auto fn_ty = std::make_shared<rc::ir::FunctionType>(
      std::make_shared<rc::ir::VoidType>(), std::vector<rc::ir::TypePtr>{},
      false);
  auto fn = m.create_function("f", fn_ty);

  auto entry = fn->create_block("entry");
  auto then_bb = fn->create_block("then");
  auto else_bb = fn->create_block("else");

  entry->append<rc::ir::BranchInst>(rc::ir::ConstantInt::get_i1(true), then_bb.get(),
                                   else_bb.get());
  then_bb->append<rc::ir::ReturnInst>();
  else_bb->append<rc::ir::ReturnInst>();

  rc::opt::DeadCodeElimVisitor dce;
  dce.run(m);

  auto *br = get_branch(*entry);
  if (!br) {
    record_failure("[dce] missing branch in entry (true case)");
    return;
  }
  if (br->is_conditional()) {
    record_failure("[dce] expected entry branch to be unconditional (true case)");
    return;
  }
  if (br->dest() != then_bb.get()) {
    record_failure("[dce] expected entry branch to target 'then' (true case)");
    return;
  }

  // The else block should become unreachable once it is no longer a successor.
  if (!get_unreachable(*else_bb)) {
    record_failure("[dce] expected else block to be squashed to unreachable (true case)");
  }
}

void test_fold_false_branch() {
  rc::ir::Module m("m");
  auto fn_ty = std::make_shared<rc::ir::FunctionType>(
      std::make_shared<rc::ir::VoidType>(), std::vector<rc::ir::TypePtr>{},
      false);
  auto fn = m.create_function("f", fn_ty);

  auto entry = fn->create_block("entry");
  auto then_bb = fn->create_block("then");
  auto else_bb = fn->create_block("else");

  entry->append<rc::ir::BranchInst>(rc::ir::ConstantInt::get_i1(false), then_bb.get(),
                                   else_bb.get());
  then_bb->append<rc::ir::ReturnInst>();
  else_bb->append<rc::ir::ReturnInst>();

  rc::opt::DeadCodeElimVisitor dce;
  dce.run(m);

  auto *br = get_branch(*entry);
  if (!br) {
    record_failure("[dce] missing branch in entry (false case)");
    return;
  }
  if (br->is_conditional()) {
    record_failure("[dce] expected entry branch to be unconditional (false case)");
    return;
  }
  if (br->dest() != else_bb.get()) {
    record_failure("[dce] expected entry branch to target 'else' (false case)");
    return;
  }

  if (!get_unreachable(*then_bb)) {
    record_failure("[dce] expected then block to be squashed to unreachable (false case)");
  }
}

void test_adce_removes_dead_chain() {
  rc::ir::Module m("m");
  auto fn_ty = std::make_shared<rc::ir::FunctionType>(
      std::make_shared<rc::ir::VoidType>(), std::vector<rc::ir::TypePtr>{},
      false);
  auto fn = m.create_function("f", fn_ty);
  auto entry = fn->create_block("entry");

  auto add = entry->append<rc::ir::BinaryOpInst>(
      rc::ir::BinaryOpKind::ADD, rc::ir::ConstantInt::get_i32(1),
      rc::ir::ConstantInt::get_i32(2), rc::ir::IntegerType::i32());
  entry->append<rc::ir::BinaryOpInst>(rc::ir::BinaryOpKind::MUL, add,
                                      rc::ir::ConstantInt::get_i32(8),
                                      rc::ir::IntegerType::i32());
  entry->append<rc::ir::ReturnInst>();

  rc::opt::DeadCodeElimVisitor dce;
  dce.run(m);

  if (count_binary_ops(*entry) != 0) {
    record_failure("[dce] expected ADCE to remove unused arithmetic chain");
  }
}

void test_adce_keeps_store_dependency() {
  rc::ir::Module m("m");
  auto fn_ty = std::make_shared<rc::ir::FunctionType>(
      std::make_shared<rc::ir::VoidType>(), std::vector<rc::ir::TypePtr>{},
      false);
  auto fn = m.create_function("f", fn_ty);
  auto entry = fn->create_block("entry");

  auto slot = entry->append<rc::ir::AllocaInst>(rc::ir::IntegerType::i32());
  auto add = entry->append<rc::ir::BinaryOpInst>(
      rc::ir::BinaryOpKind::ADD, rc::ir::ConstantInt::get_i32(40),
      rc::ir::ConstantInt::get_i32(2), rc::ir::IntegerType::i32());
  entry->append<rc::ir::StoreInst>(add, slot);
  entry->append<rc::ir::ReturnInst>();

  rc::opt::DeadCodeElimVisitor dce;
  dce.run(m);

  if (count_binary_ops(*entry) != 1) {
    record_failure("[dce] expected ADCE to keep arithmetic used by a store");
  }
}

} // namespace

int main() {
  try {
    test_fold_true_branch();
    test_fold_false_branch();
    test_adce_removes_dead_chain();
    test_adce_keeps_store_dependency();
  } catch (const std::exception &ex) {
    record_failure(std::string("[dce] unexpected exception: ") + ex.what());
  }

  if (failures != 0) {
    std::cerr << "DCE CI tests failed: " << failures << " case(s).\n";
  }
  return failures == 0 ? 0 : 1;
}
