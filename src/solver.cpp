#include "solver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <set>
#include <unordered_set>

namespace {

using Clock = std::chrono::steady_clock;

int cheapestType(const Problem& P, int count, long long len) {
    int best = -1;
    for (int t = 0; t < (int)P.lib.size(); ++t) {
        const BufType& b = P.lib[t];
        if (b.fanout >= count && b.length >= len && (best < 0 || b.cost < P.lib[best].cost))
            best = t;
    }
    return best;
}

// Re-select the cheapest legal library type for every buffer.
bool retype(const Problem& P, Tree& T) {
    const int n = (int)P.sinks.size();
    const int m = (int)T.bufs.size();
    std::vector<int> cnt(m, 0);
    std::vector<long long> len(m, 0);
    for (int id = 1; id < n + 1 + m; ++id) {
        int par = T.parent[id];
        if (par > n) {
            cnt[par - n - 1]++;
            len[par - n - 1] += manhattan(T.bufs[par - n - 1].p, nodePoint(P, T, id));
        }
    }
    for (int k = 0; k < m; ++k) {
        int t = cheapestType(P, cnt[k], len[k]);
        if (t < 0) return false;
        T.bufs[k].type = t;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Bottom-up clustering builder
// ---------------------------------------------------------------------------
struct Builder {
    const Problem& P;
    int n, cap, maxF = 0;
    long long maxL = 0;
    std::vector<Point> pts;
    std::vector<int> parent;
    std::vector<Buffer> bufs;
    std::set<std::pair<int, int>> occ;

    Builder(const Problem& p, int c) : P(p), n((int)p.sinks.size()), cap(c) {
        pts.push_back(P.src);
        for (auto& s : P.sinks) pts.push_back(s);
        parent.assign(n + 1, -1);
        for (auto& q : pts) occ.insert({q.x, q.y});
        for (auto& t : P.lib) { maxF = std::max(maxF, t.fanout); maxL = std::max(maxL, t.length); }
        cap = std::min(cap, maxF);
    }

    bool inChip(Point c) const { return c.x >= 0 && c.x <= P.dimX && c.y >= 0 && c.y <= P.dimY; }

    // Nearest unoccupied integer point inside the chip.
    bool freeSpot(Point want, Point& out) const {
        want.x = std::max(0, std::min(P.dimX, want.x));
        want.y = std::max(0, std::min(P.dimY, want.y));
        int maxR = P.dimX + P.dimY;
        for (int r = 0; r <= maxR; ++r)
            for (int dx = -r; dx <= r; ++dx) {
                int rem = r - std::abs(dx);
                for (int s = 0; s < 2; ++s) {
                    if (s == 1 && rem == 0) continue;
                    Point c{want.x + dx, want.y + (s ? -rem : rem)};
                    if (inChip(c) && !occ.count({c.x, c.y})) { out = c; return true; }
                }
            }
        return false;
    }

    long long lenSum(Point p, const std::vector<int>& ids) const {
        long long s = 0;
        for (int id : ids) s += manhattan(p, pts[id]);
        return s;
    }

    int create(Point pos, int type, const std::vector<int>& kids) {
        int id = (int)pts.size();
        pts.push_back(pos);
        parent.push_back(-1);
        bufs.push_back({type, pos});
        occ.insert({pos.x, pos.y});
        for (int c : kids) parent[c] = id;
        return id;
    }

    // Recursive bisection along the wider axis until every part has <= cap items.
    void bisect(std::vector<int> ids, int limit, std::vector<std::vector<int>>& out) const {
        if ((int)ids.size() <= limit) { out.push_back(ids); return; }
        int x0 = 1 << 30, x1 = -1, y0 = 1 << 30, y1 = -1;
        for (int id : ids) {
            x0 = std::min(x0, pts[id].x); x1 = std::max(x1, pts[id].x);
            y0 = std::min(y0, pts[id].y); y1 = std::max(y1, pts[id].y);
        }
        bool byX = (x1 - x0) >= (y1 - y0);
        std::sort(ids.begin(), ids.end(), [&](int a, int b) {
            if (byX) return pts[a].x != pts[b].x ? pts[a].x < pts[b].x : pts[a].y < pts[b].y;
            return pts[a].y != pts[b].y ? pts[a].y < pts[b].y : pts[a].x < pts[b].x;
        });
        size_t mid = ids.size() / 2;
        bisect(std::vector<int>(ids.begin(), ids.begin() + mid), limit, out);
        bisect(std::vector<int>(ids.begin() + mid, ids.end()), limit, out);
    }

    // Try to drive group g with one buffer at its median; on failure split it.
    // Nodes that cannot be grouped are passed through to `out` unchanged.
    void makeGroup(const std::vector<int>& g, std::vector<int>& out) {
        if (g.size() == 1) { out.push_back(g[0]); return; }
        std::vector<int> xs, ys;
        for (int id : g) { xs.push_back(pts[id].x); ys.push_back(pts[id].y); }
        std::sort(xs.begin(), xs.end());
        std::sort(ys.begin(), ys.end());
        Point want{xs[xs.size() / 2], ys[ys.size() / 2]}, pos;
        if (freeSpot(want, pos)) {
            int t = cheapestType(P, (int)g.size(), lenSum(pos, g));
            if (t >= 0) { out.push_back(create(pos, t, g)); return; }
        }
        if (g.size() > 2) {
            std::vector<std::vector<int>> parts;
            bisect(g, ((int)g.size() + 1) / 2, parts);
            for (auto& part : parts) makeGroup(part, out);
        } else {
            for (int id : g) out.push_back(id);
        }
    }

    long long srcLen(const std::vector<int>& cur) const {
        long long s = 0;
        for (int id : cur) s += manhattan(P.src, pts[id]);
        return s;
    }

    static Point stepToward(Point a, Point b, int t) {
        int dx = std::min(t, std::abs(b.x - a.x));
        a.x += (b.x >= a.x ? dx : -dx);
        t -= dx;
        int dy = std::min(t, std::abs(b.y - a.y));
        a.y += (b.y >= a.y ? dy : -dy);
        return a;
    }

    // SRC has enough fanout but not enough length: insert relay buffers on the
    // longest SRC edges (each relay drives exactly the node it replaces).
    bool relayRepair(std::vector<int>& cur) {
        for (int it = 0; it < 100000; ++it) {
            if (srcLen(cur) <= P.srcLength) return true;
            int bi = 0, bd = -1;
            for (int i = 0; i < (int)cur.size(); ++i) {
                int d = manhattan(P.src, pts[cur[i]]);
                if (d > bd) { bd = d; bi = i; }
            }
            if (bd <= 1) return false;
            int s = cur[bi];
            int t = (int)std::min<long long>(maxL, bd - 1);
            Point pos;
            if (!freeSpot(stepToward(pts[s], P.src, t), pos)) return false;
            int dNew = manhattan(P.src, pos), dKid = manhattan(pos, pts[s]);
            if (dNew >= bd) return false;
            int ty = cheapestType(P, 1, dKid);
            if (ty < 0) return false;
            cur[bi] = create(pos, ty, {s});
        }
        return false;
    }

    bool build(Tree& T) {
        if (P.srcFanout < 1 || cap < 2) return false;
        std::vector<int> cur;
        for (int i = 1; i <= n; ++i) cur.push_back(i);
        for (int iter = 0; iter < 200; ++iter) {
            if ((int)cur.size() <= P.srcFanout) {
                if (srcLen(cur) <= P.srcLength || relayRepair(cur)) {
                    for (int c : cur) parent[c] = 0;
                    T.bufs = bufs;
                    T.parent = parent;
                    return true;
                }
            }
            if (cur.size() < 2) return false;
            std::vector<std::vector<int>> groups;
            bisect(cur, cap, groups);
            std::vector<int> nxt;
            for (auto& g : groups) makeGroup(g, nxt);
            if (nxt.size() >= cur.size()) return false;  // no progress
            cur = nxt;
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
// Local search moves (each candidate is re-typed and fully re-validated)
// ---------------------------------------------------------------------------
void removeBuffer(Tree& T, int n, int k) {
    int id = n + 1 + k, par = T.parent[id];
    for (int x = 1; x < (int)T.parent.size(); ++x)
        if (T.parent[x] == id) T.parent[x] = par;
    T.parent.erase(T.parent.begin() + id);
    T.bufs.erase(T.bufs.begin() + k);
    for (int x = 1; x < (int)T.parent.size(); ++x)
        if (T.parent[x] > id) T.parent[x]--;
}

bool timeUp(const Clock::time_point& deadline) { return Clock::now() >= deadline; }

bool removalPass(const Problem& P, Tree& T, EvalResult& best, const Clock::time_point& dl) {
    const int n = (int)P.sinks.size();
    bool any = false;
    for (int k = (int)T.bufs.size() - 1; k >= 0 && !timeUp(dl); --k) {
        Tree C = T;
        removeBuffer(C, n, k);
        if (!retype(P, C)) continue;
        EvalResult r = evaluate(P, C);
        if (r.legal && r.score < best.score) { T = C; best = r; any = true; }
    }
    return any;
}

bool movePass(const Problem& P, Tree& T, EvalResult& best, const Clock::time_point& dl) {
    static const int dxs[4] = {1, -1, 0, 0}, dys[4] = {0, 0, 1, -1};
    static const int steps[5] = {16, 8, 4, 2, 1};
    bool any = false;
    for (int k = 0; k < (int)T.bufs.size() && !timeUp(dl); ++k)
        for (int st : steps)
            for (int d = 0; d < 4; ++d) {
                Tree C = T;
                C.bufs[k].p.x += dxs[d] * st;
                C.bufs[k].p.y += dys[d] * st;
                if (!retype(P, C)) continue;
                EvalResult r = evaluate(P, C);
                if (r.legal && r.score < best.score) { T = C; best = r; any = true; }
            }
    return any;
}


// ---------------------------------------------------------------------------
// Fast O(N) evaluation used inside simulated annealing.
// Re-types every buffer to the cheapest feasible type and returns the Score.
// Distinct-coordinate / in-chip / acyclic properties are guaranteed by the
// move generators, and the final tree is re-validated with evaluate().
// ---------------------------------------------------------------------------
struct Scratch {
    std::vector<int> cnt, stk;
    std::vector<long long> len, arr;
    std::vector<char> done;
};

bool fastEval(const Problem& P, Tree& T, Scratch& S, long long& score) {
    const int n = (int)P.sinks.size();
    const int m = (int)T.bufs.size();
    const int N = 1 + n + m;
    auto pt = [&](int id) -> const Point& {
        return id == 0 ? P.src : (id <= n ? P.sinks[id - 1] : T.bufs[id - n - 1].p);
    };
    S.cnt.assign(N, 0);
    S.len.assign(N, 0);
    for (int id = 1; id < N; ++id) {
        int par = T.parent[id];
        S.cnt[par]++;
        S.len[par] += manhattan(pt(par), pt(id));
    }
    if (S.cnt[0] > P.srcFanout || S.len[0] > P.srcLength) return false;
    long long cost = 0;
    for (int k = 0; k < m; ++k) {
        int id = n + 1 + k;
        if (S.cnt[id] == 0) return false;  // unused buffer
        int t = cheapestType(P, S.cnt[id], S.len[id]);
        if (t < 0) return false;
        T.bufs[k].type = t;
        cost += P.lib[t].cost;
    }
    S.arr.assign(N, 0);
    S.done.assign(N, 0);
    S.done[0] = 1;
    for (int id = 1; id < N; ++id) {
        if (S.done[id]) continue;
        S.stk.clear();
        int x = id;
        while (!S.done[x]) {
            S.stk.push_back(x);
            x = T.parent[x];
            if ((int)S.stk.size() > N) return false;  // cycle guard
        }
        while (!S.stk.empty()) {
            int y = S.stk.back();
            S.stk.pop_back();
            int par = T.parent[y];
            S.arr[y] = S.arr[par] + manhattan(pt(par), pt(y));
            S.done[y] = 1;
        }
    }
    long long tmax = S.arr[1], tmin = S.arr[1];
    for (int i = 1; i <= n; ++i) {
        tmax = std::max(tmax, S.arr[i]);
        tmin = std::min(tmin, S.arr[i]);
    }
    score = cost + (long long)P.wSkew * (tmax - tmin);
    return true;
}

// Remove buffers that lost all their children (cascading upwards).
void pruneEmpty(Tree& T, int n) {
    for (;;) {
        int m = (int)T.bufs.size();
        std::vector<int> cnt(1 + n + m, 0);
        for (int id = 1; id < 1 + n + m; ++id) cnt[T.parent[id]]++;
        int victim = -1;
        for (int k = m - 1; k >= 0; --k)
            if (cnt[n + 1 + k] == 0) { victim = k; break; }
        if (victim < 0) return;
        removeBuffer(T, n, victim);
    }
}

typedef unsigned long long u64;
inline u64 keyOf(Point p) { return ((u64)(unsigned)p.x << 32) | (unsigned)p.y; }

// ---------------------------------------------------------------------------
// Simulated annealing. Moves: move buffer, reassign a node to another parent,
// swap the parents of two sinks, insert a buffer above a few siblings, remove
// a buffer. Only legal candidates are ever accepted; the best legal tree seen
// is returned.
// ---------------------------------------------------------------------------
void annealing(const Problem& P, Tree& best, long long& bestScore,
               const Clock::time_point& startT, const Clock::time_point& endT) {
    const int n = (int)P.sinks.size();
    if (n < 2) return;
    std::mt19937_64 rng(12345);
    auto rnd = [&](int lo, int hi) { return lo + (int)(rng() % (u64)(hi - lo + 1)); };
    auto unif = [&]() { return (double)(rng() >> 11) / 9007199254740992.0; };

    Scratch S;
    Tree cur = best, C;
    long long curScore = 0;
    if (!fastEval(P, cur, S, curScore)) return;
    bestScore = curScore;
    best = cur;

    std::unordered_set<u64> occ;
    auto buildOcc = [&](const Tree& T) {
        occ.clear();
        occ.insert(keyOf(P.src));
        for (auto& s : P.sinks) occ.insert(keyOf(s));
        for (auto& b : T.bufs) occ.insert(keyOf(b.p));
    };
    buildOcc(cur);

    const int maxDim = std::max(P.dimX, P.dimY);
    double prog = 0.0;
    int mvK = -1;
    Point mvOld, mvNew;

    auto inChip = [&](Point q) { return q.x >= 0 && q.x <= P.dimX && q.y >= 0 && q.y <= P.dimY; };

    // returns 0 = no move, 1 = buffer position move, 2 = structural move
    auto propose = [&](Tree& T) -> int {
        const int m = (int)T.bufs.size();
        const int N = 1 + n + m;
        int r = rnd(0, 99);
        if (r < 40) {  // move buffer
            if (m == 0) return 0;
            int k = rnd(0, m - 1);
            Point o = T.bufs[k].p;
            int maxStep = std::max(2, (int)(maxDim / 6.0 * (1.0 - prog)));
            int s = (rnd(0, 1) ? rnd(1, 2) : rnd(1, maxStep));
            Point q{o.x + rnd(-s, s), o.y + rnd(-s, s)};
            if ((q.x == o.x && q.y == o.y) || !inChip(q) || occ.count(keyOf(q))) return 0;
            T.bufs[k].p = q;
            mvK = k; mvOld = o; mvNew = q;
            return 1;
        }
        if (r < 65) {  // reassign node x to a (nearby) parent
            int x = rnd(1, N - 1);
            int bq = -1, bd = 1 << 30;
            for (int t = 0; t < 6; ++t) {
                int ri = rnd(0, m);
                int q = (ri == 0) ? 0 : n + ri;
                if (q == x || q == T.parent[x]) continue;
                if (x > n) {
                    bool bad = false;
                    for (int y = q; y != 0; y = T.parent[y]) if (y == x) { bad = true; break; }
                    if (bad) continue;
                }
                int d = manhattan(nodePoint(P, T, q), nodePoint(P, T, x));
                if (d < bd) { bd = d; bq = q; }
            }
            if (bq < 0) return 0;
            T.parent[x] = bq;
            pruneEmpty(T, n);
            return 2;
        }
        if (r < 80) {  // swap parents of two sinks
            int a = rnd(1, n), bb = -1, bd = 1 << 30;
            for (int t = 0; t < 6; ++t) {
                int b = rnd(1, n);
                if (b == a || T.parent[b] == T.parent[a]) continue;
                int d = manhattan(P.sinks[a - 1], P.sinks[b - 1]);
                if (d < bd) { bd = d; bb = b; }
            }
            if (bb < 0) return 0;
            std::swap(T.parent[a], T.parent[bb]);
            return 2;
        }
        if (r < 90) {  // insert a buffer above 2-3 sibling nodes
            int ri = rnd(0, m);
            int u = (ri == 0) ? 0 : n + ri;
            std::vector<int> kids;
            for (int id = 1; id < N; ++id) if (T.parent[id] == u) kids.push_back(id);
            if (kids.size() < 2) return 0;
            int a = kids[rnd(0, (int)kids.size() - 1)];
            std::sort(kids.begin(), kids.end(), [&](int x, int y) {
                return manhattan(nodePoint(P, T, a), nodePoint(P, T, x)) <
                       manhattan(nodePoint(P, T, a), nodePoint(P, T, y));
            });
            int g = std::min<int>(kids.size(), rnd(2, 3));
            std::vector<int> xs, ys;
            for (int i = 0; i < g; ++i) {
                Point q = nodePoint(P, T, kids[i]);
                xs.push_back(q.x); ys.push_back(q.y);
            }
            std::sort(xs.begin(), xs.end());
            std::sort(ys.begin(), ys.end());
            Point want{xs[g / 2], ys[g / 2]}, pos;
            bool found = false;
            for (int rr = 0; rr <= 4 && !found; ++rr)
                for (int dx = -rr; dx <= rr && !found; ++dx) {
                    int rem = rr - std::abs(dx);
                    for (int s = 0; s < 2 && !found; ++s) {
                        if (s == 1 && rem == 0) continue;
                        Point c{want.x + dx, want.y + (s ? -rem : rem)};
                        if (inChip(c) && !occ.count(keyOf(c))) { pos = c; found = true; }
                    }
                }
            if (!found) return 0;
            T.bufs.push_back({0, pos});
            int id = n + 1 + m;
            T.parent.push_back(u);
            for (int i = 0; i < g; ++i) T.parent[kids[i]] = id;
            return 2;
        }
        // remove a buffer
        if (m == 0) return 0;
        removeBuffer(T, n, rnd(0, m - 1));
        return 2;
    };

    // Calibrate the starting temperature from typical |delta| of legal moves.
    double sum = 0; int cntD = 0;
    for (int tries = 0; tries < 2000 && cntD < 300; ++tries) {
        C = cur;
        if (!propose(C)) continue;
        long long sc;
        if (!fastEval(P, C, S, sc)) continue;
        long long d = sc - curScore;
        if (d != 0) { sum += (double)std::llabs(d); ++cntD; }
    }
    const double T0 = cntD ? std::max(1.0, sum / cntD) : 1.0;
    const double Tend = std::max(0.05, T0 * 0.01);

    const long long maxIters = 30000LL * (n + 10);
    const double dur = std::chrono::duration<double>(endT - startT).count();
    long long it = 0, sinceBest = 0;
    double temp = T0;
    for (;; ++it) {
        if ((it & 63) == 0) {
            double el = std::chrono::duration<double>(Clock::now() - startT).count();
            prog = std::max(el / dur, (double)it / (double)maxIters);
            if (prog >= 1.0) break;
            temp = T0 * std::pow(Tend / T0, prog);
        }
        C = cur;
        int kind = propose(C);
        if (!kind) continue;
        long long sc;
        if (!fastEval(P, C, S, sc)) continue;
        long long d = sc - curScore;
        if (d <= 0 || unif() < std::exp(-(double)d / temp)) {
            cur = C;
            curScore = sc;
            if (kind == 1) { occ.erase(keyOf(mvOld)); occ.insert(keyOf(mvNew)); }
            else buildOcc(cur);
            if (curScore < bestScore) { bestScore = curScore; best = cur; sinceBest = 0; }
        }
        if (++sinceBest > 200000) {  // restart from the best solution
            cur = best; curScore = bestScore; buildOcc(cur); sinceBest = 0;
        }
    }
}

}  // namespace

SolveOutput solve(const Problem& P, double timeLimitSec) {
    const auto deadline = Clock::now() + std::chrono::milliseconds((long long)(timeLimitSec * 1000));
    const int n = (int)P.sinks.size();
    SolveOutput out;
    bool have = false;
    Tree bestTree;
    EvalResult best;

    auto consider = [&](const Tree& T, const std::string& name) {
        EvalResult r = evaluate(P, T);
        out.stages.push_back({name, r});
        if (r.legal && (!have || r.score < best.score)) { have = true; bestTree = T; best = r; }
    };

    // Candidate 0: SRC drives every sink directly.
    Tree direct;
    direct.parent.assign(n + 1, 0);
    direct.parent[0] = -1;
    consider(direct, "candidate: direct SRC->sinks");

    // Candidates: clustering with different group-size caps.
    std::set<int> caps;
    int maxF = 0;
    for (auto& t : P.lib) { caps.insert(t.fanout); maxF = std::max(maxF, t.fanout); }
    for (int c = 2; c <= std::min(maxF, 16); ++c) caps.insert(c);
    for (int c : caps) {
        if (c < 2) continue;
        Builder b(P, c);
        Tree T;
        if (b.build(T)) consider(T, "candidate: bisect cap=" + std::to_string(c));
    }

    if (!have) {
        out.ok = false;
        return out;
    }
    out.stages.push_back({"initial (best legal candidate)", best});

    // Stage 2a: greedy local search (time-boxed).
    const auto t0 = Clock::now();
    auto at = [&](double f) { return t0 + std::chrono::milliseconds((long long)(timeLimitSec * f * 1000)); };
    Tree T = bestTree;
    {
        const auto dl = at(0.25);
        bool improved = true;
        while (improved && !timeUp(dl)) {
            improved = false;
            if (removalPass(P, T, best, dl)) improved = true;
            if (movePass(P, T, best, dl)) improved = true;
        }
        out.stages.push_back({"after greedy local search", evaluate(P, T)});
    }

    // Stage 2b: simulated annealing (reassign / swap / insert / remove / move).
    {
        Tree A = T;
        long long sc = best.score;
        annealing(P, A, sc, Clock::now(), at(0.92));
        EvalResult r = evaluate(P, A);
        if (r.legal && r.score <= best.score) { T = A; best = r; }
        out.stages.push_back({"after simulated annealing", evaluate(P, T)});
    }

    // Stage 2c: final greedy polish with the remaining time.
    {
        bool improved = true;
        while (improved && !timeUp(deadline)) {
            improved = false;
            if (removalPass(P, T, best, deadline)) improved = true;
            if (movePass(P, T, best, deadline)) improved = true;
        }
        out.stages.push_back({"after final polish", evaluate(P, T)});
    }
    out.tree = T;
    out.res = evaluate(P, T);
    out.ok = out.res.legal;
    return out;
}
