#pragma once
#include "semantic/error/exceptions.hpp"
#include "semantic/analyzer/exprAnalyzer.hpp"
#include "semantic/analyzer/patternAnalyzer.hpp"
#include "semantic/analyzer/stmtAnalyzer.hpp"
#include "semantic/analyzer/symbolTable.hpp"
#include "semantic/analyzer/typeAnalyzer.hpp"

#include <memory>

namespace rc {

class SemanticContext {
public:
  SemanticContext();

  SymbolTable &symbols();
  const SymbolTable &symbols() const;

  TypeAnalyzer &types();
  const TypeAnalyzer &types() const;

  ExprAnalyzer &exprs();
  StmtAnalyzer &stmts();
  PatternAnalyzer &patterns();

private:
  SymbolTable symbol_table_;
  TypeAnalyzer type_analyzer_;
  ExprAnalyzer expr_analyzer_;
  StmtAnalyzer stmt_analyzer_;
  PatternAnalyzer pattern_analyzer_;
};

} // namespace rc