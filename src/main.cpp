#include <iostream>
#include <string>
#include <vector>

#include "ast/parser.hpp"
#include "lexer/lexer.hpp"
#include "preprocessor/preprocessor.hpp"

int main() {
  try {
    rc::Preprocessor preprocessor("../examples/hello_world.rs");
    auto preprocessed_code = preprocessor.preprocess();

    rc::Lexer lexer(preprocessed_code);
    auto tks = lexer.tokenize();

    std::cout << "Tokens:" << std::endl;

    for (const auto &token : tks) {
      std::cout << "Token Type: " << token.type << ", Lexeme: " << token.lexeme
                << "\n";
    }

    rc::Parser parser(tks);
    auto ast = parser.parse();

    if (ast) {
      std::cout << ast->children.size() << " top-level items." << std::endl;
    } else {
      std::cout << "Failed!" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
