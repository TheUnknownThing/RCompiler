#pragma once

#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rc {
class Preprocessor {
public:
  Preprocessor(const std::string &filename);
  ~Preprocessor() = default;

  std::string preprocess();

private:
  std::vector<std::string> file_lines;
};

} // namespace rc