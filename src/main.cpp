#define LOGGING_LEVEL_DEBUG

#include <sstream>
#include <string>
#include <vector>

#include "ast/parser.hpp"
#include "ast/visitors/pretty_print.hpp"
#include "ir/gen.hpp"
#include "ir/visit.hpp"
#include "lexer/lexer.hpp"
#include "opt/cfg/visit.hpp"
#include "preprocessor/preprocessor.hpp"
#include "semantic/semantic.hpp"
#include "utils/logger.hpp"

int main(int argc, char *argv[]) {
  try {
    std::string filename = "";
    if (argc > 1) {
      filename = argv[1];
    }
    rc::Preprocessor preprocessor(filename);
    auto preprocessed_code = preprocessor.preprocess();

    rc::Lexer lexer(preprocessed_code);
    auto tks = lexer.tokenize();

    LOG_INFO("[Tokens] Processed complete. Total tokens: " +
             std::to_string(tks.size()));

    size_t pos = 0;
    for (const auto &token : tks) {
      std::ostringstream oss;
      oss << "Pos:" << pos++ << "\t Type: " << token.type
          << "\t Lexeme: " << token.lexeme;
      LOG_DEBUG(oss.str());
    }

    rc::Parser parser(tks);
    auto ast = parser.parse();

    if (ast) {
      LOG_INFO(std::string("Parsed ") + std::to_string(ast->children.size()) +
               " top-level items.");
      LOG_DEBUG("[AST Pretty Print]");
      LOG_DEBUG("\n" + rc::pretty_print(*ast));

      rc::SemanticAnalyzer analyzer;
      analyzer.analyze(ast);

      auto *root_scope = analyzer.root_scope();

      try {
        rc::ir::Context irCtx(analyzer.expr_cache());
        rc::ir::IREmitter emitter;
        emitter.run(ast, root_scope, irCtx);
        rc::ir::emitLLVM(emitter.module(), std::cout);
        // opt pass
        rc::opt::IRVisitor irVisitor;
        irVisitor.run(emitter.module());
      } catch (...) {
        LOG_ERROR("Error during IR generation");
        return 0;
      }
    }

  } catch (const std::exception &e) {
    LOG_ERROR(std::string("Error: ") + e.what());
    return 1;
  }

  return 0;
}
