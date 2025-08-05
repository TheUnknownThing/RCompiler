#include "../../include/preprocessor/preprocessor.hpp"

int main() {
  rc::Preprocessor p("../../examples/comments.rs");
  auto processed_lines = p.preprocess();

  for (const std::string &line : processed_lines) {
    std::cout << line << std::endl;
  }

  return 0;
}