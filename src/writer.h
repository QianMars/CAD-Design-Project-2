#pragma once
#include <string>
#include "types.h"

// Writes .buffer / .level sections. Creates the output directory if needed.
bool writeTree(const std::string& path, const Problem& P, const Tree& T, std::string& err);
