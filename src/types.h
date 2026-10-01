#pragma once
#include <cstdlib>
#include <string>
#include <vector>

struct Point {
    int x = 0, y = 0;
};

inline int manhattan(const Point& a, const Point& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

struct BufType {
    std::string name;
    int fanout = 0;
    long long length = 0;
    int cost = 0;
};

struct Problem {
    int srcFanout = 0;
    long long srcLength = 0;
    int dimX = 0, dimY = 0;
    std::vector<BufType> lib;
    int wSkew = 1;
    Point src;
    std::vector<Point> sinks;  // sinks[i] is S(i+1)
};

struct Buffer {
    int type = 0;  // index into Problem::lib
    Point p;
};

// Node ids: 0 = SRC, 1..n = S1..Sn, n+1..n+m = B1..Bm
// parent[id] is the driver of node id (parent[0] = -1).
struct Tree {
    std::vector<Buffer> bufs;
    std::vector<int> parent;
};
