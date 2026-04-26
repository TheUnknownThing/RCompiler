#include "semantic/semantic.hpp"

namespace rc {

SemanticAnalyzer::SemanticAnalyzer() = default;
void SemanticAnalyzer::analyze(const std::shared_ptr<RootNode> &root) {
  // First pass collects item name
  FirstPassBuilder first;
  auto prelude_owner = first.build(root);

  // Second pass resolves semantic type and evaluates constant expressions
  SecondPassResolver second;
  second.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Third pass promotes impl to struct level
  ThirdPassPromoter third;
  third.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Fourth pass handles let statements and bindings
  FourthPass fourth;
  fourth.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  // Control analyzer analysis inappropriate continues and breaks
  ControlAnalyzer control_analyzer;
  control_analyzer.analyze(root);

  // Dirty work is just dirty work
  DirtyWorkPass dirty_work;
  dirty_work.run(std::dynamic_pointer_cast<RootNode>(root), first.root_scope);

  prelude_scope_owner_ = std::move(prelude_owner);
  root_scope_ = first.root_scope;
  expr_cache_ = fourth.get_expr_cache();
}

} // namespace rc
