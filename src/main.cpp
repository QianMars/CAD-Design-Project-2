#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "evaluator.h"
#include "parser.h"
#include "solver.h"
#include "writer.h"

// Usage: cbi INPUT_FILE OUTPUT_FILE
// Set CBI_VERBOSE=1 to print per-stage statistics to stderr (for the report).
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Error: not enough arguments.\nUsage: " << (argc > 0 ? argv[0] : "cbi")
                  << " INPUT_FILE OUTPUT_FILE\n";
        return 1;
    }
    Problem P;
    std::string err;
    if (!parseProblem(argv[1], P, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    // Time budget (hard limit is 60 s per test case). CBI_TIME overrides it for experiments.
    double timeLimit = 40.0;
    if (const char* e = std::getenv("CBI_TIME")) timeLimit = std::atof(e);
    SolveOutput S = solve(P, timeLimit);
    if (!S.ok) {
        std::cerr << "Error: no legal clock tree found\n";
        return 2;
    }
    if (!writeTree(argv[2], P, S.tree, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    if (std::getenv("CBI_VERBOSE")) {
        for (const auto& s : S.stages) {
            if (s.res.legal)
                std::fprintf(stderr, "%-34s cost=%-5lld skew=%-6lld score=%lld\n", s.name.c_str(),
                             s.res.cost, s.res.skew(), s.res.score);
            else
                std::fprintf(stderr, "%-34s ILLEGAL (%s)\n", s.name.c_str(), s.res.msg.c_str());
        }
    }
    std::printf("T_max: %lld, T_min: %lld, Score: %lld\n", S.res.tmax, S.res.tmin, S.res.score);
    return 0;
}
