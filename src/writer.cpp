#include "writer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "evaluator.h"

bool writeTree(const std::string& path, const Problem& P, const Tree& T, std::string& err) {
    namespace fs = std::filesystem;
    try {
        fs::path parent = fs::path(path).parent_path();
        if (!parent.empty()) fs::create_directories(parent);
    } catch (const std::exception& e) {
        err = std::string("cannot create output directory: ") + e.what();
        return false;
    }
    std::ofstream out(path);
    if (!out) { err = "cannot open output file: " + path; return false; }

    const int n = (int)P.sinks.size();
    const int m = (int)T.bufs.size();
    const int N = 1 + n + m;

    out << ".buffer " << m << " # number of clock buffers\n";
    for (int k = 0; k < m; ++k) {
        const Buffer& b = T.bufs[k];
        out << "B" << (k + 1) << " " << P.lib[b.type].name << " " << b.p.x << " " << b.p.y
            << " #clock buffer B" << (k + 1) << "\n";
    }
    out << ".e\n\n";

    std::vector<std::vector<int>> ch(N);
    for (int id = 1; id < N; ++id) ch[T.parent[id]].push_back(id);
    // buffers first, then sinks; each by id
    for (auto& v : ch)
        std::sort(v.begin(), v.end(), [&](int a, int b) {
            bool ba = a > n, bb = b > n;
            if (ba != bb) return ba;
            return a < b;
        });

    // BFS by level: level k lists the edges (k-1) -> k
    std::vector<std::vector<int>> levels;
    std::vector<int> frontier{0};
    while (!frontier.empty()) {
        std::vector<int> parents, next;
        for (int u : frontier) {
            if (ch[u].empty()) continue;
            parents.push_back(u);
            for (int c : ch[u]) if (c > n) next.push_back(c);
        }
        if (parents.empty()) break;
        levels.push_back(parents);
        frontier = next;
    }

    out << ".level " << levels.size() << " # number of tree levels\n";
    for (size_t k = 0; k < levels.size(); ++k) {
        out << (k + 1);
        for (size_t j = 0; j < levels[k].size(); ++j) {
            int u = levels[k][j];
            out << " " << nodeName(n, u) << ":{";
            for (size_t c = 0; c < ch[u].size(); ++c)
                out << (c ? " " : "") << nodeName(n, ch[u][c]);
            out << "}";
        }
        out << "\n";
    }
    out << ".e\n";
    return true;
}
