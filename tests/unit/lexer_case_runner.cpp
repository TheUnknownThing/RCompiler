#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>

#include "../../include/preprocessor/preprocessor.hpp"
#include "../../include/lexer/lexer.hpp"

static std::vector<std::string> read_lines(const std::string &path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("Failed to open file: " + path);
  }
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) lines.push_back(line);
  }
  return lines;
}

static bool should_print_lexeme(rc::TokenType t) {
  switch (t) {
    case rc::TokenType::NON_KEYWORD_IDENTIFIER:
    case rc::TokenType::INTEGER_LITERAL:
    case rc::TokenType::CHAR_LITERAL:
    case rc::TokenType::STRING_LITERAL:
    case rc::TokenType::C_STRING_LITERAL:
    case rc::TokenType::BYTE_STRING_LITERAL:
    case rc::TokenType::BYTE_LITERAL:
    case rc::TokenType::UNKNOWN:
      return true;
    default:
      return false;
  }
}

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <input.rs> <expected.tokens>\n";
    return 2;
  }
  const std::string input_rs = argv[1];
  const std::string expected_tokens = argv[2];

  try {
    rc::Preprocessor pre(input_rs);
    auto preprocessed = pre.preprocess();

    rc::Lexer lexer(preprocessed);
    auto tokens = lexer.tokenize();

    // Build actual token lines in the same format as .tokens examples
    std::vector<std::string> actual;
    actual.reserve(tokens.size());
    for (const auto &tk : tokens) {
      std::ostringstream os;
      os << tk.type;
      if (should_print_lexeme(tk.type)) {
        os << '(' << tk.lexeme << ')';
      }
      actual.push_back(os.str());
    }

    auto expected = read_lines(expected_tokens);

    // Compare sizes and lines, print diffs if any
    bool ok = true;
    if (actual.size() != expected.size()) {
      ok = false;
      std::cerr << "Token count mismatch: expected " << expected.size()
                << ", got " << actual.size() << "\n";
    }

    const size_t n = std::min(actual.size(), expected.size());
    for (size_t i = 0; i < n; ++i) {
      if (actual[i] != expected[i]) {
        ok = false;
        std::cerr << "Mismatch at line " << (i + 1) << ":\n"
                  << "  expected: " << expected[i] << "\n"
                  << "  actual  : " << actual[i] << "\n";
      }
    }

    if (!ok) {
      std::cerr << "\nFull actual tokens:\n";
      for (const auto &l : actual) std::cerr << l << "\n";
      return 1;
    }

    std::cout << "Lexer test passed for: " << input_rs << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
