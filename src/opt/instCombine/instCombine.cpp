#include "opt/instCombine/instCombine.hpp"

namespace rc::opt {

void InstCombinePass::registerDefaultRules() {
  addRule(std::make_unique<BinaryConstFoldRule>());
  addRule(std::make_unique<BinaryIdentityRule>());
  addRule(std::make_unique<ICmpFoldRule>());
  addRule(std::make_unique<SelectFoldRule>());
  addRule(std::make_unique<TrivialCastRule>());
}

} // namespace rc::opt
