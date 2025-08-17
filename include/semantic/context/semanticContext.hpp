#pragma once
#include "semantic/analyzer/exprAnalyzer.hpp"
#include "semantic/analyzer/patternAnalyzer.hpp"
#include "semantic/analyzer/stmtAnalyzer.hpp"
#include "semantic/analyzer/symbolTable.hpp"
#include "semantic/analyzer/typeAnalyzer.hpp"
#include "semantic/error/exceptions.hpp"

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
  SymbolTable symbol_table;
  TypeAnalyzer type_analyzer;
  ExprAnalyzer expr_analyzer;
  StmtAnalyzer stmt_analyzer;
  PatternAnalyzer pattern_analyzer;
};

inline SemanticContext::SemanticContext()
    : symbol_table(), type_analyzer(*this), expr_analyzer(*this), stmt_analyzer(*this),
      pattern_analyzer(*this) {}

inline SymbolTable &SemanticContext::symbols() { return symbol_table; }
inline const SymbolTable &SemanticContext::symbols() const {
  return symbol_table;
}

inline TypeAnalyzer &SemanticContext::types() { return type_analyzer; }
inline const TypeAnalyzer &SemanticContext::types() const {
  return type_analyzer;
}

inline ExprAnalyzer &SemanticContext::exprs() { return expr_analyzer; }
inline StmtAnalyzer &SemanticContext::stmts() { return stmt_analyzer; }
inline PatternAnalyzer &SemanticContext::patterns() {
  return pattern_analyzer;
}

} // namespace rc