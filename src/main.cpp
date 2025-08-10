#define LOGGING_LEVEL_DEBUG

#include <iostream>
#include <string>
#include <vector>

#include "utils/logger.hpp"
#include "ast/parser.hpp"
#include "ast/pretty_print.hpp"
#include "lexer/lexer.hpp"
#include "preprocessor/preprocessor.hpp"

int main() {
  try {
    rc::Preprocessor preprocessor("../examples/ast/20.rs");
    auto preprocessed_code = preprocessor.preprocess();

    rc::Lexer lexer(preprocessed_code);
    auto tks = lexer.tokenize();

    std::cout << "Tokens:" << std::endl;

    size_t pos = 0;
    for (const auto &token : tks) {
      std::cout << "Pos:" << pos++ << "\t Type: " << token.type << "\t\t Lexeme: " << token.lexeme
                << "\n";
    }

    rc::Parser parser(tks);
    auto ast = parser.parse();

    if (ast) {
      std::cout << "\nParsed" << ast->children.size() << " top-level items."
                << std::endl;
      std::cout << "\nAST Pretty Print:" << std::endl;
      std::cout << rc::pretty_print(*ast) << std::endl;
    } else {
      std::cout << "Failed!" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
