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

  std::string preprocess();

private:
  std::vector<std::string> file_lines;
};

inline Preprocessor::Preprocessor(const std::string &filename) {
  file_lines.clear();

  if (filename.empty()) {
    // Read from stdin
    std::string line;
    while (std::getline(std::cin, line)) {
      file_lines.push_back(line);
    }
  } else {
    // Read from file
    std::ifstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file: " + filename);
    }
    std::string line;
    while (std::getline(file, line)) {
      file_lines.push_back(line);
    }
  }
}

inline Preprocessor::~Preprocessor() { file_lines.clear(); }

inline std::string Preprocessor::preprocess() {
  std::string result;

  int in_block_comment = 0;
  bool in_string = false;
  bool in_char = false;
  bool escaped = false;

  for (size_t line_idx = 0; line_idx < file_lines.size(); ++line_idx) {
    const std::string &line = file_lines[line_idx];

    for (size_t i = 0; i < line.length(); ++i) {
      char c = line[i];
      char next_c = (i + 1 < line.length()) ? line[i + 1] : '\0';

      if (in_block_comment) {
        if (c == '*' && next_c == '/') {
          in_block_comment--;
          if (in_block_comment == 0) {
            if (!result.empty() && result.back() != ' ') {
              result += " ";
            }
          }
          i++;
        } else if (c == '/' && next_c == '*') {
          in_block_comment++;
          i++;
        }
      } else if (in_string) {
        // string literals
        result += c;
        if (escaped) {
          escaped = false;
        } else if (c == '\\') {
          escaped = true;
        } else if (c == '"') {
          in_string = false;
        }
      } else if (in_char) {
        // char literals
        result += c;
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
          // entering string literal
          in_string = true;
          result += c;
        } else if (c == '\'') {
          in_char = true;
          result += c;
        } else {
          // whitespace and tabs
          if (std::isspace(c)) {
            if (!result.empty() && result.back() != ' ') {
              result += " ";
            }
          } else {
            result += c;
          }
        }
      }
    }

    // line breaks
    if (line_idx < file_lines.size() - 1) { // Not the last line
      if (in_string) {
        result += "\\n";
      } else if (!in_block_comment && !result.empty() && result.back() != ' ') {
        result += " ";
      }
    }
  }

  std::string cleaned_result;
  bool prev_space = false;
  for (char c : result) {
    if (c == ' ') {
      if (!prev_space) {
        cleaned_result += c;
        prev_space = true;
      }
    } else {
      cleaned_result += c;
      prev_space = false;
    }
  }

  size_t start = cleaned_result.find_first_not_of(' ');
  if (start == std::string::npos) {
    return {""};
  }
  size_t end = cleaned_result.find_last_not_of(' ');
  cleaned_result = cleaned_result.substr(start, end - start + 1);

  return cleaned_result;
}
} // namespace rc