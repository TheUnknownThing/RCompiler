#pragma once

#include "ast/nodes/pattern.hpp"
#include "ast/types.hpp"
#include "semantic/analyzer/symbolTable.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rc {

class SemanticContext;

struct PatternBinding {
  std::string name;
  LiteralType type;
  bool is_mutable = false;
};

class PatternAnalyzer : public BaseVisitor {
public:
  explicit PatternAnalyzer(SemanticContext &ctx);

  std::vector<PatternBinding> analyze(const std::shared_ptr<BasePattern> &pat,
                                      const LiteralType &expectedType);

  void visit(BaseNode &node) override;

  void visit(IdentifierPattern &) override;
  void visit(LiteralPattern &) override;
  void visit(WildcardPattern &) override;
  void visit(RestPattern &) override;
  void visit(ReferencePattern &) override;
  void visit(StructPattern &) override;
  void visit(TuplePattern &) override;
  void visit(GroupedPattern &) override;
  void visit(PathPattern &) override;
  void visit(SlicePattern &) override;
  void visit(OrPattern &) override;

  const std::vector<PatternBinding> &bindings() const;

private:
  SemanticContext &ctx;
  std::vector<PatternBinding> current_bindings;

  void addBinding(std::string name, const LiteralType &ty, bool is_mutable);
};

inline PatternAnalyzer::PatternAnalyzer(SemanticContext &ctx) : ctx(ctx) {}

} // namespace rc
