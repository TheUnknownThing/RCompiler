#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../include/preprocessor/preprocessor.hpp"
#include "../../include/lexer/lexer.hpp"
#include "../../include/ast/parser.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <input.rs>\n";
    return 2;
  }

  const std::string input_rs = argv[1];
  try {
    rc::Preprocessor pre(input_rs);
    auto preprocessed = pre.preprocess();

    rc::Lexer lexer(preprocessed);
    auto tokens = lexer.tokenize();

    rc::Parser parser(tokens);
    auto ast = parser.parse();

    if (!ast) {
      std::cerr << "Parser returned null AST for: " << input_rs << "\n";
      return 1;
    }

    std::cout << "Parser smoke passed for: " << input_rs
              << ", top-level items: " << ast->children.size() << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
