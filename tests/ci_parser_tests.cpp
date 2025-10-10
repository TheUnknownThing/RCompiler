#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ast/parser.hpp"
#include "lexer/lexer.hpp"
#include "preprocessor/preprocessor.hpp"

namespace fs = std::filesystem;

namespace {
int failures = 0;
void record_failure(const std::string &message) {
  ++failures;
  std::cerr << message << '\n';
}

std::set<std::string> read_known_list(const fs::path &path) {
  std::set<std::string> entries;
  if (!fs::exists(path)) {
    return entries;
  }

  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      entries.insert(line);
    }
  }
  return entries;
}

struct ParseResult {
  bool success{false};
  bool threw{false};
  std::string error_message;
};

ParseResult attempt_parse(const fs::path &path) {
  try {
    rc::Preprocessor preprocessor(path.string());
    const auto preprocessed = preprocessor.preprocess();
    rc::Lexer lexer(preprocessed);
    const auto tokens = lexer.tokenize();
    rc::Parser parser(tokens);
    auto root = parser.parse();

    if (!root) {
      return {.success = false, .threw = false,
              .error_message = "parser returned null root"};
    }

    return {.success = true, .threw = false, .error_message = ""};
  } catch (const std::exception &ex) {
    return {.success = false, .threw = true, .error_message = ex.what()};
  }
}

void run_directory(const fs::path &dir, bool expect_success) {
  if (!fs::exists(dir)) {
    std::cerr << "Parser fixtures directory missing: " << dir << '\n';
    failures++;
    return;
  }

  std::vector<fs::path> files;
  for (const auto &entry : fs::directory_iterator(dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".rs") {
      continue;
    }
    files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());

  for (const auto &file : files) {
    const auto filename = file.filename().string();
    const auto result = attempt_parse(file);

    if (expect_success) {
      if (result.success) {
        continue;
      }

      std::ostringstream oss;
      oss << "[parser] Valid source failed: " << filename;
      if (!result.error_message.empty()) {
        oss << " :: " << result.error_message;
      }
      record_failure(oss.str());
      continue;
    } else {
      if (!result.success) {
        continue;
      }

      std::ostringstream oss;
      oss << "[parser] Invalid source parsed successfully: " << filename;
      record_failure(oss.str());
      continue;
    }

  }
}
} // namespace

int main() {
  const fs::path project_root{PROJECT_ROOT_DIR};
  const fs::path valid_dir = project_root / "ci/files/parser/valid";
  const fs::path invalid_dir = project_root / "ci/files/parser/invalid";
  const fs::path expected_dir = project_root / "ci/expected_output/parser";

  run_directory(valid_dir, /*expect_success=*/true);
  run_directory(invalid_dir, /*expect_success=*/false);

  if (failures != 0) {
    std::cerr << "Parser CI tests failed: " << failures << " case(s).\n";
  }
  return failures == 0 ? 0 : 1;
}
