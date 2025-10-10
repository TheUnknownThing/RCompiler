#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "preprocessor/preprocessor.hpp"

namespace fs = std::filesystem;

namespace {
int failures = 0;

void record_failure(const std::string &message) {
  ++failures;
  std::cerr << message << '\n';
}

std::string read_file(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Failed to open file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void run_case(const fs::path &input_path, const fs::path &expected_path) {
  try {
    rc::Preprocessor preprocessor(input_path.string());
    const auto actual = preprocessor.preprocess();
    const auto expected = read_file(expected_path);

    if (actual != expected) {
      std::ostringstream oss;
      oss << "[preprocessor] Output mismatch for " << input_path.filename()
          << "\nExpected: " << expected << "\nActual  : " << actual;
      record_failure(oss.str());
    }
  } catch (const std::exception &ex) {
    std::ostringstream oss;
    oss << "[preprocessor] Exception for " << input_path.filename() << ": "
        << ex.what();
    record_failure(oss.str());
  }
}
} // namespace

int main() {
  const fs::path project_root{PROJECT_ROOT_DIR};
  const fs::path input_dir = project_root / "ci/files/preprocessor";
  const fs::path expected_dir = project_root / "ci/expected_output/preprocessor";

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

  for (const auto &input_path : cases) {
    const fs::path expected_path =
        expected_dir / (input_path.stem().string() + ".txt");
    if (!fs::exists(expected_path)) {
      std::ostringstream oss;
      oss << "[preprocessor] Missing expectation for "
          << input_path.filename();
      record_failure(oss.str());
      continue;
    }
    run_case(input_path, expected_path);
  }

  if (failures != 0) {
    std::cerr << "Preprocessor CI tests failed: " << failures << " case(s).\n";
  }
  return failures == 0 ? 0 : 1;
}
