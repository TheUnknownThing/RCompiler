#pragma once

#include <string>
#include <vector>

#include "ast/types.hpp"

namespace rc {

// Replace `Self` Type with given target name
AstType replace_self(AstType t, const std::string &target_name);

} // namespace rc
