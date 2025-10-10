#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lexer/lexer.hpp"
#include "preprocessor/preprocessor.hpp"

namespace fs = std::filesystem;

namespace {
int failures = 0;

void record_failure(const std::string &message) {
  ++failures;
  std::cerr << message << '\n';
}

std::vector<std::string> read_lines(const fs::path &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Failed to open file: " + path.string());
  }
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(line);
  }
  return lines;
}

void run_case(const fs::path &source_path, const fs::path &expected_path) {
  try {
    rc::Preprocessor preprocessor(source_path.string());
    const auto preprocessed = preprocessor.preprocess();
    rc::Lexer lexer(preprocessed);
    const auto tokens = lexer.tokenize();

    const auto expected_tokens = read_lines(expected_path);

    if (tokens.size() != expected_tokens.size()) {
      std::ostringstream oss;
      oss << "[lexer] Token count mismatch for " << source_path.filename()
          << ": expected " << expected_tokens.size() << ", got "
          << tokens.size();
      record_failure(oss.str());
      return;
    }

    for (std::size_t i = 0; i < tokens.size(); ++i) {
      const auto actual_type = rc::toString(tokens[i].type);
      if (actual_type != expected_tokens[i]) {
        std::ostringstream oss;
        oss << "[lexer] Token mismatch for " << source_path.filename()
            << " at index " << i << ": expected '" << expected_tokens[i]
            << "', got '" << actual_type << "' (lexeme='"
            << tokens[i].lexeme << "')";
        record_failure(oss.str());
        break;
      }
    }
  } catch (const std::exception &ex) {
    std::ostringstream oss;
    oss << "[lexer] Exception for " << source_path.filename() << ": "
        << ex.what();
    record_failure(oss.str());
  }
}
} // namespace

int main() {
  const fs::path project_root{PROJECT_ROOT_DIR};
  const fs::path input_dir = project_root / "ci/files/lexer";
  const fs::path expected_dir = project_root / "ci/expected_output/lexer";

  if (!fs::exists(input_dir)) {
    std::cerr << "Input directory missing: " << input_dir << '\n';
    return 1;
  }
  if (!fs::exists(expected_dir)) {
    std::cerr << "Expected directory missing: " << expected_dir << '\n';
    return 1;
  }

  std::vector<fs::path> cases;
  for (const auto &entry : fs::directory_iterator(input_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".rs") {
      continue;
    }
    cases.push_back(entry.path());
  }
  std::sort(cases.begin(), cases.end());

  for (const auto &source_path : cases) {
    const auto expected_path =
        expected_dir / (source_path.stem().string() + ".tokens.txt");
    if (!fs::exists(expected_path)) {
      std::ostringstream oss;
      oss << "[lexer] Missing expectation for " << source_path.filename();
      record_failure(oss.str());
      continue;
    }
    run_case(source_path, expected_path);
  }

  if (failures != 0) {
    std::cerr << "Lexer CI tests failed: " << failures << " case(s).\n";
  }
  return failures == 0 ? 0 : 1;
}
