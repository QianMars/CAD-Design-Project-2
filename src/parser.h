#pragma once
#include <string>
#include "types.h"

// Reads a .cbi input file. Returns false and fills err on failure.
bool parseProblem(const std::string& path, Problem& P, std::string& err);
