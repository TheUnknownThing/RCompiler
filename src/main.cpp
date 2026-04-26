#include <string>
#include <vector>

#include "ast/parser.hpp"
#include "backend/pass_manager.hpp"
#include "ir/gen.hpp"
#include "ir/visit.hpp"
#include "lexer/lexer.hpp"
#include "opt/pass_manager.hpp"
#include "preprocessor/preprocessor.hpp"
#include "semantic/semantic.hpp"
#include "utils/logger.hpp"

int main(int argc, char *argv[]) {
  try {
    std::string filename = "";
    enum class EmitMode { LLVM, ASM };
    EmitMode emit_mode = EmitMode::LLVM;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--emit-llvm") {
        emit_mode = EmitMode::LLVM;
      } else if (arg == "--emit-asm" || arg == "-S") {
        emit_mode = EmitMode::ASM;
      } else if (filename.empty()) {
        filename = arg;
      } else {
        throw std::runtime_error("unexpected argument: " + arg);
      }
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

    rc::ir::Context ir_ctx(analyzer.expr_cache());
    rc::ir::IREmitter emitter;
    emitter.run(ast, root_scope, ir_ctx);

    rc::opt::ConstantContext const_ctx;
    rc::opt::PassManager pm(const_ctx);
    pm.run(emitter.module());

    if (emit_mode == EmitMode::LLVM) {
      rc::ir::emit_llvm(emitter.module(), std::cout);
    } else {
      rc::backend::PassManager backend_pm;
      backend_pm.run(emitter.module(), std::cout);
    }

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    LOG_ERROR(std::string("Error: ") + e.what());
    return 1;
  }

  return 0;
}
