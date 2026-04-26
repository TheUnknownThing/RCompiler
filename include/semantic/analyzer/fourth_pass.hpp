#pragma once

#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/pattern.hpp"
#include "ast/nodes/stmt.hpp"
#include "ast/nodes/top_level.hpp"
#include "ast/types.hpp"
#include "semantic/analyzer/builtin.hpp"
#include "semantic/error/exceptions.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"
#include "utils/logger.hpp"

namespace rc {

class FourthPass : public BaseVisitor {
public:
  FourthPass() = default;
  ~FourthPass() = default;

  // Entry point
  void run(const std::shared_ptr<RootNode> &root, ScopeNode *root_scope_);

  SemType evaluate(Expression *expr);

  void visit(BaseNode &node) override;
  void visit(RootNode &node) override;
  void visit(FunctionDecl &node) override;
  void visit(ConstantItem &node) override;
  void visit(StructDecl &) override;
  void visit(EnumDecl &) override;
  void visit(TraitDecl &) override;
  void visit(ImplDecl &) override;
  void visit(LetStatement &node) override;
  void visit(ExpressionStatement &node) override;
  void visit(EmptyStatement &) override;
  void visit(NameExpression &node) override;
  void visit(LiteralExpression &node) override;
  void visit(PrefixExpression &node) override;
  void visit(BinaryExpression &node) override;
  void visit(GroupExpression &node) override;
  void visit(IfExpression &node) override;
  void visit(ReturnExpression &node) override;
  void visit(CallExpression &node) override;
  void visit(MethodCallExpression &node) override;
  void visit(FieldAccessExpression &node) override;
  void visit(StructExpression &node) override;
  void visit(UnderscoreExpression &node) override;
  void visit(BlockExpression &node) override;
  void visit(LoopExpression &node) override;
  void visit(WhileExpression &node) override;
  void visit(ArrayExpression &node) override;
  void visit(IndexExpression &node) override;
  void visit(TupleExpression &) override;
  void visit(BreakExpression &node) override;
  void visit(ContinueExpression &node) override;
  void visit(PathExpression &node) override;
  void visit(QualifiedPathExpression &) override;
  void visit(BorrowExpression &node) override;
  void visit(DerefExpression &node) override;

  auto get_expr_cache() -> const std::unordered_map<const BaseNode *, SemType> & {
    return expr_cache;
  }

private:
  struct IdentifierMeta {
    bool is_mutable;
    bool is_ref;
    SemType type;
  };

  struct PlaceInfo {
    bool is_place = false;
    bool is_writable = false;
    std::optional<std::string> root_name;
  };

  using BindingFrame = std::map<std::string, IdentifierMeta>;
  std::vector<BindingFrame> binding_stack;

  std::optional<std::pair<std::vector<std::shared_ptr<BasePattern>>,
                          std::vector<SemType>>>
      pending_params;

  ScopeNode *current_scope_node = nullptr;

  std::unordered_map<const BaseNode *, SemType> expr_cache;

  std::vector<SemType> function_return_stack;

  std::vector<SemType> loop_break_stack;
  const CollectedItem *current_impl_type = nullptr;
  const CollectedItem *current_struct_type = nullptr;

  void cache_expr(const BaseNode *n, SemType t);
  std::optional<SemType> lookup_binding(const std::string &name) const;
  const IdentifierMeta *lookup_binding_meta(const std::string &name) const;
  void add_binding(const std::string &name, const IdentifierMeta &meta);
  void extract_pattern_bindings(const BasePattern &pattern,
                                const SemType &type);
  void extract_identifier_pattern(const IdentifierPattern &pattern,
                                  const SemType &type);
  void extract_reference_pattern(const ReferencePattern &pattern,
                                 const SemType &type);
  bool is_integer(SemPrimitiveKind k) const;
  bool is_str(SemPrimitiveKind k) const;
  void require_bool(const SemType &t, const std::string &msg);
  void require_integer(const SemType &t, const std::string &msg);
  void require_bool_or_integer(const SemType &t, const std::string &msg);
  std::optional<SemType> unify_integers(const SemType &a,
                                        const SemType &b) const;
  std::optional<SemType> unify_for_op(const SemType &a, const SemType &b,
                                      bool allow_str = false) const;
  bool can_assign(const SemType &dst, const SemType &src) const;
  SemType eval_binary(BinaryExpression &bin);
  bool is_assignment_token(TokenType tt) const;
  std::optional<TokenType> compound_base_operator(TokenType tt) const;
  PlaceInfo analyze_place(Expression *expr);
  void require_place_writable(Expression *lhs, const char *context);
  void handle_assignment(BinaryExpression &node);
  void validate_irrefutable_pattern(const BasePattern &pattern);
  SemType primitive_kind_from_name(Expression *e);
  bool is_integer_primitive_kind(SemPrimitiveKind k) const;
  bool is_integer_type(const SemType &t) const;
  bool is_cast_allowed(const SemType &src, const SemType &dst) const;
  void handle_as_cast(BinaryExpression &node);
  void resolve_path_function_call(const PathExpression &pe,
                                  CallExpression &node);
  SemType auto_deref(const SemType &t);
  ScopeNode *find_scope_for_owner(const BaseNode *owner,
                                  ScopeNode *start) const;
};

// Implementation

} // namespace rc
