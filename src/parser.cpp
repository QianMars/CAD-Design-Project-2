#include "parser.h"

#include <fstream>
#include <sstream>

namespace {
std::vector<std::string> tokenize(std::istream& in) {
    std::vector<std::string> toks;
    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find('#');
        if (pos != std::string::npos) line.erase(pos);
        std::istringstream ss(line);
        std::string t;
        while (ss >> t) toks.push_back(t);
    }
    return toks;
}
}  // namespace

bool parseProblem(const std::string& path, Problem& P, std::string& err) {
    std::ifstream fin(path);
    if (!fin) {
        err = "cannot open input file: " + path;
        return false;
    }
    auto tk = tokenize(fin);
    size_t i = 0;
    auto more = [&]() { return i < tk.size(); };
    auto nextStr = [&]() -> std::string { return more() ? tk[i++] : std::string(); };
    auto nextInt = [&](long long& v) -> bool {
        if (!more()) return false;
        try {
            size_t used = 0;
            v = std::stoll(tk[i], &used);
            if (used != tk[i].size()) return false;
        } catch (...) {
            return false;
        }
        ++i;
        return true;
    };

    bool gotPin = false;
    long long v = 0;
    while (more()) {
        std::string t = nextStr();
        if (t == ".limit") {
            while (more()) {
                std::string k = nextStr();
                if (k == ".e") break;
                if (k == "fanout") {
                    if (!nextInt(v)) { err = "bad fanout"; return false; }
                    P.srcFanout = (int)v;
                } else if (k == "length") {
                    if (!nextInt(v)) { err = "bad length"; return false; }
                    P.srcLength = v;
                } else if (k == ".dimx") {
                    if (!nextInt(v)) { err = "bad .dimx"; return false; }
                    P.dimX = (int)v;
                } else if (k == ".dimy") {
                    if (!nextInt(v)) { err = "bad .dimy"; return false; }
                    P.dimY = (int)v;
                }
            }
        } else if (t == ".dimx") {
            if (!nextInt(v)) { err = "bad .dimx"; return false; }
            P.dimX = (int)v;
        } else if (t == ".dimy") {
            if (!nextInt(v)) { err = "bad .dimy"; return false; }
            P.dimY = (int)v;
        } else if (t == ".buflib") {
            long long cnt;
            if (!nextInt(cnt)) { err = "bad .buflib count"; return false; }
            for (long long k = 0; k < cnt; ++k) {
                BufType b;
                long long f, l, c;
                b.name = nextStr();
                if (b.name.empty() || !nextInt(f) || !nextInt(l) || !nextInt(c)) {
                    err = "bad buffer library entry";
                    return false;
                }
                b.fanout = (int)f;
                b.length = l;
                b.cost = (int)c;
                P.lib.push_back(b);
            }
            if (nextStr() != ".e") { err = ".buflib missing .e"; return false; }
        } else if (t == ".objective") {
            while (more()) {
                std::string k = nextStr();
                if (k == ".e") break;
                if (k == "w_skew") {
                    if (!nextInt(v)) { err = "bad w_skew"; return false; }
                    P.wSkew = (int)v;
                }
            }
        } else if (t == ".pin") {
            long long cnt;
            if (!nextInt(cnt) || cnt < 1) { err = "bad .pin count"; return false; }
            for (long long k = 0; k < cnt; ++k) {
                long long x, y;
                if (!nextInt(x) || !nextInt(y)) { err = "bad pin coordinate"; return false; }
                Point p{(int)x, (int)y};
                if (k == 0) P.src = p; else P.sinks.push_back(p);
            }
            if (nextStr() != ".e") { err = ".pin missing .e"; return false; }
            gotPin = true;
        }
        // unknown tokens are skipped
    }
    if (!gotPin) { err = "missing .pin section"; return false; }
    if (P.lib.empty()) { err = "empty buffer library"; return false; }
    if (P.dimX <= 0 || P.dimY <= 0) { err = "missing chip dimension"; return false; }
    return true;
}
