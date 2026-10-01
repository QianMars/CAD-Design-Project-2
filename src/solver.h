#pragma once
#include <string>
#include <vector>

#include "evaluator.h"
#include "types.h"

struct Stage {
    std::string name;
    EvalResult res;
};

struct SolveOutput {
    bool ok = false;
    Tree tree;
    EvalResult res;
    std::vector<Stage> stages;  // for the report: skew / cost / score per stage
};

// Stage 1: build legal candidates (direct, bottom-up bisection clustering with
//          several group-size caps) and keep the best legal one.
// Stage 2: legality-preserving local search (remove buffers, move buffers).
SolveOutput solve(const Problem& P, double timeLimitSec);
