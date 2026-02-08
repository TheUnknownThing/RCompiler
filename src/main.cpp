#define LOGGING_LEVEL_NONE

#include <sstream>
#include <string>
#include <vector>

#include "ast/parser.hpp"
#include "ast/visitors/pretty_print.hpp"
#include "ir/gen.hpp"
#include "ir/visit.hpp"
#include "lexer/lexer.hpp"
#include "opt/passManager.hpp"
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
    rc::Parser parser(tks);
    auto ast = parser.parse();

    if (!ast) {
      throw std::runtime_error("Parsing failed: AST is null");
    }
    
    rc::SemanticAnalyzer analyzer;
    analyzer.analyze(ast);

    auto *root_scope = analyzer.root_scope();

    rc::ir::Context irCtx(analyzer.expr_cache());
    rc::ir::IREmitter emitter;
    emitter.run(ast, root_scope, irCtx);

    rc::opt::ConstantContext constCtx;
    rc::opt::PassManager pm(constCtx);
    pm.run(emitter.module());

    rc::ir::emitLLVM(emitter.module(), std::cout);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    LOG_ERROR(std::string("Error: ") + e.what());
    return 1;
  }

  return 0;
}
