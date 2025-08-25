#define LOGGING_LEVEL_DEBUG

#include <iostream>
#include <string>
#include <vector>

#include "ast/parser.hpp"
#include "ast/visitors/pretty_print.hpp"
#include "lexer/lexer.hpp"
#include "preprocessor/preprocessor.hpp"
#include "semantic/analyzer/firstPass.hpp"
#include "semantic/analyzer/secondPass.hpp"
#include "semantic/semantic.hpp"
#include "utils/logger.hpp"

int main() {
  try {
    rc::Preprocessor preprocessor("");
    auto preprocessed_code = preprocessor.preprocess();

    rc::Lexer lexer(preprocessed_code);
    auto tks = lexer.tokenize();

    std::cout << "Tokens:" << std::endl;

    size_t pos = 0;
    for (const auto &token : tks) {
      std::cout << "Pos:" << pos++ << "\t Type: " << token.type
                << "\t\t Lexeme: " << token.lexeme << "\n";
    }

    rc::Parser parser(tks);
    auto ast = parser.parse();

    if (ast) {
      std::cout << "\nParsed" << ast->children.size() << " top-level items."
                << std::endl;
      std::cout << "\nAST Pretty Print:" << std::endl;
      std::cout << rc::pretty_print(*ast) << std::endl;

      rc::FirstPassBuilder first_pass;
      first_pass.build(ast);
      std::cout
          << "\n[Semantic] First pass (scope & item collection) completed."
          << std::endl;

      rc::SecondPassBuilder second_pass(first_pass);
      second_pass.build(ast);
      std::cout << "[Semantic] Second pass (impl promotion) completed."
                << std::endl;

      // // Legacy symbol table build (can be removed once fully migrated)
      // rc::SymbolTable symtab;
      // rc::SymbolChecker symChecker(symtab);
      // symChecker.build(ast);
      // std::cout << "[Semantic] Legacy symbol table build completed." <<
      // std::endl;

      // rc::SemanticContext semCtx;
      // semCtx.symbols() = symtab;
      // // Type checking / expression analysis / statement analysis
      // // rc::TypeAnalyzer &types = semCtx.types();
      // // rc::ExprAnalyzer &exprs = semCtx.exprs();
      // // rc::StmtAnalyzer &stmts = semCtx.stmts();
      // // rc::PatternAnalyzer &patterns = semCtx.patterns();
    } else {
      std::cout << "Failed!" << std::endl;
    }

  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
