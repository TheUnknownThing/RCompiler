#include "opt/inst_combine/inst_combine.hpp"

namespace rc::opt {

void InstCombinePass::register_default_rules() {
  add_rule(std::make_unique<BinaryConstFoldRule>());
  add_rule(std::make_unique<BinaryIdentityRule>());
  add_rule(std::make_unique<StrengthReductionRule>());
  add_rule(std::make_unique<ICmpFoldRule>());
  add_rule(std::make_unique<SelectFoldRule>());
  add_rule(std::make_unique<TrivialCastRule>());
}

} // namespace rc::opt
