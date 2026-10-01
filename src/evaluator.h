#pragma once
#include <string>
#include "types.h"

struct EvalResult {
    bool legal = false;
    std::string msg;  // reason if illegal
    long long tmax = 0, tmin = 0, cost = 0, score = 0;
    long long skew() const { return tmax - tmin; }
};

std::string nodeName(int n, int id);
Point nodePoint(const Problem& P, const Tree& T, int id);

// Full legality check + Tmax/Tmin/Score computation.
EvalResult evaluate(const Problem& P, const Tree& T);
