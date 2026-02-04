#include <iostream>
#include <sstream>
#include <string>

#include "ir/instructions/controlFlow.hpp"
#include "ir/instructions/topLevel.hpp"
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

void test_fold_true_branch() {
  rc::ir::Module m("m");
  auto fnTy = std::make_shared<rc::ir::FunctionType>(
      std::make_shared<rc::ir::VoidType>(), std::vector<rc::ir::TypePtr>{},
      false);
  auto fn = m.createFunction("f", fnTy);

  auto entry = fn->createBlock("entry");
  auto thenBB = fn->createBlock("then");
  auto elseBB = fn->createBlock("else");

  entry->append<rc::ir::BranchInst>(rc::ir::ConstantInt::getI1(true), thenBB,
                                   elseBB);
  thenBB->append<rc::ir::ReturnInst>();
  elseBB->append<rc::ir::ReturnInst>();

  rc::opt::DeadCodeElimVisitor dce;
  dce.run(m);

  auto *br = get_branch(*entry);
  if (!br) {
    record_failure("[dce] missing branch in entry (true case)");
    return;
  }
  if (br->isConditional()) {
    record_failure("[dce] expected entry branch to be unconditional (true case)");
    return;
  }
  if (br->dest().get() != thenBB.get()) {
    record_failure("[dce] expected entry branch to target 'then' (true case)");
    return;
  }

  // The else block should become unreachable once it is no longer a successor.
  if (!get_unreachable(*elseBB)) {
    record_failure("[dce] expected else block to be squashed to unreachable (true case)");
  }
}

void test_fold_false_branch() {
  rc::ir::Module m("m");
  auto fnTy = std::make_shared<rc::ir::FunctionType>(
      std::make_shared<rc::ir::VoidType>(), std::vector<rc::ir::TypePtr>{},
      false);
  auto fn = m.createFunction("f", fnTy);

  auto entry = fn->createBlock("entry");
  auto thenBB = fn->createBlock("then");
  auto elseBB = fn->createBlock("else");

  entry->append<rc::ir::BranchInst>(rc::ir::ConstantInt::getI1(false), thenBB,
                                   elseBB);
  thenBB->append<rc::ir::ReturnInst>();
  elseBB->append<rc::ir::ReturnInst>();

  rc::opt::DeadCodeElimVisitor dce;
  dce.run(m);

  auto *br = get_branch(*entry);
  if (!br) {
    record_failure("[dce] missing branch in entry (false case)");
    return;
  }
  if (br->isConditional()) {
    record_failure("[dce] expected entry branch to be unconditional (false case)");
    return;
  }
  if (br->dest().get() != elseBB.get()) {
    record_failure("[dce] expected entry branch to target 'else' (false case)");
    return;
  }

  if (!get_unreachable(*thenBB)) {
    record_failure("[dce] expected then block to be squashed to unreachable (false case)");
  }
}

} // namespace

int main() {
  try {
    test_fold_true_branch();
    test_fold_false_branch();
  } catch (const std::exception &ex) {
    record_failure(std::string("[dce] unexpected exception: ") + ex.what());
  }

  if (failures != 0) {
    std::cerr << "DCE CI tests failed: " << failures << " case(s).\n";
  }
  return failures == 0 ? 0 : 1;
}
