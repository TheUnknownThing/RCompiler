#pragma once

#include <map>
#include <string>

namespace rc {
class Parser {
public:
  Parser(const std::string &source);
  ~Parser();

  void parse();

private:
  std::string source;
};
} // namespace rc