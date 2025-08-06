#include "../../include/lexer/lexer.hpp"
#include "../../include/preprocessor/preprocessor.hpp"
#include <assert.h>

int main() {
  rc::Preprocessor p("../../examples/lexer/strange_number.rs");
  auto processed_lines = p.preprocess();
  rc::Lexer lexer(processed_lines);
  auto tokens = lexer.tokenize();

  std::cout << "Original Source:\n" << processed_lines << "\n\n";

  for (const auto &token : tokens) {
    std::cout << "Token Type: " << token.type << ", Lexeme: " << token.lexeme
              << "\n";
  }
  return 0;
}