#pragma once

#include "gen.hpp"
#include "visit.hpp"

namespace rc {

class IRGenerator {
public:
  IRGenerator();

  void generate();
};

inline void IRGenerator::generate() {}

} // namespace rc