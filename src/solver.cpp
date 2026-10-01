#include "solver.h"

#include <algorithm>
#include <chrono>
#include <set>

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

    // Stage 2: legality-preserving local search.
    Tree T = bestTree;
    bool improved = true;
    while (improved && !timeUp(deadline)) {
        improved = false;
        if (removalPass(P, T, best, deadline)) {
            improved = true;
            out.stages.push_back({"after buffer removal", best});
        }
        if (movePass(P, T, best, deadline)) {
            improved = true;
            out.stages.push_back({"after buffer moving", best});
        }
    }
    out.tree = T;
    out.res = evaluate(P, T);
    out.ok = out.res.legal;
    return out;
}
