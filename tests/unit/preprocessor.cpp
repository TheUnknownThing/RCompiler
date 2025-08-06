#include "../../include/preprocessor/preprocessor.hpp"
#include <assert.h>

int main() {
  rc::Preprocessor p("../../examples/preprocessor/comments.rs");
  auto processed_lines = p.preprocess();

  auto standard_output = R"(pub mod nested_comments { let a = "/* aaaaaaaaaaaaa */"; let b = '/'; let c = "/*"; let d = "*/"; let e = "/* not a /* nested */ comment */"; let f = "///"; let g = "\\\" /* this is not a comment */\n\n \""; let h = "\\"; let i = 12 13; let j = 12 ; } pub mod outer_module { pub mod inner_module {} pub mod degenerate_cases { pub mod dummy_item {} } })";

  std::cout << processed_lines << std::endl;

  assert(processed_lines == standard_output);

  std::cout << "Test passed!" << std::endl;
  return 0;
}