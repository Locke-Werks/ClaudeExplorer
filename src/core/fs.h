#pragma once

#include <filesystem>

// One alias, in one place, because half the tree takes paths and spelling
// std::filesystem out everywhere reads as if the choice were still open.
namespace cx {

namespace fs = std::filesystem;

} // namespace cx
