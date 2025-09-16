#pragma once

namespace rc {

// Forward declarations
class NameExpression;
class LiteralExpression;
class PrefixExpression;
class BinaryExpression;
class GroupExpression;
class IfExpression;
class MatchExpression;
class ReturnExpression;
class CallExpression;
class FieldAccessExpression;
class MethodCallExpression;
class UnderscoreExpression;
class BlockExpression;
class LoopExpression;
class WhileExpression;
class BreakExpression;
class ContinueExpression;
class ArrayExpression;
class IndexExpression;
class TupleExpression;
class StructExpression;
class BlockStatement;
class LetStatement;
class ExpressionStatement;
class EmptyStatement;
class FunctionDecl;
class ConstantItem;
class ModuleDecl;
class StructDecl;
class EnumDecl;
class TraitDecl;
class ImplDecl;
class BasePattern;
class IdentifierPattern;
class LiteralPattern;
class WildcardPattern;
class RestPattern;
class ReferencePattern;
class StructPattern;
class TuplePattern;
class GroupedPattern;
class PathPattern;
class SlicePattern;
class OrPattern;
class RootNode;
class PathExpression;
class QualifiedPathExpression;
class BorrowExpression;
class DerefExpression;

class BaseVisitor;

class BaseNode {
public:
  virtual ~BaseNode() = default;
  virtual void accept(BaseVisitor &visitor) = 0;
};

class BaseVisitor {
public:
  virtual ~BaseVisitor() = default;

  virtual void visit(BaseNode &node) = 0;

  // Expression visitors
  virtual void visit(NameExpression &) {}
  virtual void visit(LiteralExpression &) {}
  virtual void visit(PrefixExpression &) {}
  virtual void visit(BinaryExpression &) {}
  virtual void visit(GroupExpression &) {}
  virtual void visit(IfExpression &) {}
  virtual void visit(MatchExpression &) {}
  virtual void visit(ReturnExpression &) {}
  virtual void visit(CallExpression &) {}
  virtual void visit(FieldAccessExpression &) {}
  virtual void visit(MethodCallExpression &) {}
  virtual void visit(UnderscoreExpression &) {}
  virtual void visit(BlockExpression &) {}
  virtual void visit(LoopExpression &) {}
  virtual void visit(WhileExpression &) {}
  virtual void visit(BreakExpression &) {}
  virtual void visit(ContinueExpression &) {}
  virtual void visit(ArrayExpression &) {}
  virtual void visit(IndexExpression &) {}
  virtual void visit(TupleExpression &) {}
  virtual void visit(StructExpression &) {}
  virtual void visit(PathExpression &) {}
  virtual void visit(QualifiedPathExpression &) {}
  virtual void visit(BorrowExpression &) {}
  virtual void visit(DerefExpression &) {}

  // Statement visitors
  virtual void visit(BlockStatement &) {}
  virtual void visit(LetStatement &) {}
  virtual void visit(ExpressionStatement &) {}
  virtual void visit(EmptyStatement &) {}

  // Pattern visitors
  virtual void visit(BasePattern &) {}
  virtual void visit(IdentifierPattern &) {}
  virtual void visit(LiteralPattern &) {}
  virtual void visit(WildcardPattern &) {}
  virtual void visit(RestPattern &) {}
  virtual void visit(ReferencePattern &) {}
  virtual void visit(StructPattern &) {}
  virtual void visit(TuplePattern &) {}
  virtual void visit(GroupedPattern &) {}
  virtual void visit(PathPattern &) {}
  virtual void visit(SlicePattern &) {}
  virtual void visit(OrPattern &) {}

  // Top-level declaration visitors
  virtual void visit(FunctionDecl &) {}
  virtual void visit(ConstantItem &) {}
  virtual void visit(ModuleDecl &) {}
  virtual void visit(StructDecl &) {}
  virtual void visit(EnumDecl &) {}
  virtual void visit(TraitDecl &) {}
  virtual void visit(ImplDecl &) {}
  virtual void visit(RootNode &) {}
};

} // namespace rc