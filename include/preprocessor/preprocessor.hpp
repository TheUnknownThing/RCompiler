#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rc {
class Preprocessor {
public:
  Preprocessor(const std::string &filename);
  ~Preprocessor();

  std::vector<std::string> preprocess();

private:
  std::vector<std::string> file_lines;
};

inline Preprocessor::Preprocessor(const std::string &filename) {
  file_lines.clear();
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }
  std::string line;
  while (std::getline(file, line)) {
    file_lines.push_back(line);
  }
}

inline Preprocessor::~Preprocessor() { file_lines.clear(); }

inline std::vector<std::string> Preprocessor::preprocess() {
  std::vector<std::string> processed_lines;

  int in_block_comment = 0;

  for (const std::string &line : file_lines) {
    std::string processed_line;
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;

    for (size_t i = 0; i < line.length(); ++i) {
      char c = line[i];
      char next_c = (i + 1 < line.length()) ? line[i + 1] : '\0';

      if (in_block_comment) {
        if (c == '*' && next_c == '/') {
          in_block_comment--;
          if (in_block_comment == 0) {
            processed_line += " ";
          }
          i++;
        } else if (c == '/' && next_c == '*') {
          in_block_comment++;
          i++;
        }
      } else if (in_string) {
        // string literals
        processed_line += c;
        if (escaped) {
          escaped = false;
        } else if (c == '\\') {
          escaped = true;
        } else if (c == '"') {
          in_string = false;
        }
      } else if (in_char) {
        // char literals
        processed_line += c;
        if (escaped) {
          escaped = false;
        } else if (c == '\\') {
          escaped = true;
        } else if (c == '\'') {
          in_char = false;
        }
      } else {
        if (c == '/' && next_c == '/') {
          break;
        } else if (c == '/' && next_c == '*') {
          in_block_comment++;
          i++;
        } else if (c == '"') {
          // in string literal
          in_string = true;
          processed_line += c;
        } else if (c == '\'') {
          in_char = true;
          processed_line += c;
        } else {
          processed_line += c;
        }
      }
    }

    processed_lines.push_back(processed_line);
  }

  for (auto &line : processed_lines) {
    for (size_t i = 0; i < line.length(); ++i) {
      if (line[i] == '\t') {
        line[i] = ' ';
      }
      line[i] = std::isspace(line[i]) ? ' ' : line[i];
    }
    line.erase(0, line.find_first_not_of(' '));
    line.erase(line.find_last_not_of(' ') + 1);

    line.erase(std::unique(line.begin(), line.end(),
                           [](char a, char b) { return a == ' ' && b == ' '; }),
               line.end());
  }

  processed_lines.erase(
      std::remove_if(processed_lines.begin(), processed_lines.end(),
                     [](const std::string &line) { return line.empty(); }),
      processed_lines.end());

  return processed_lines;
}
} // namespace rc