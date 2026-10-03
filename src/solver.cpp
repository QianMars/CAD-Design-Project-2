#include "solver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
    int mode = 0;  // 0 = bisection grouping, 1 = nearest-neighbour grouping

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

    // Can one buffer (placed at the median, before snapping) drive group g?
    bool feasibleGroup(const std::vector<int>& g) const {
        if (g.size() == 1) return true;
        std::vector<int> xs, ys;
        for (int id : g) { xs.push_back(pts[id].x); ys.push_back(pts[id].y); }
        std::sort(xs.begin(), xs.end());
        std::sort(ys.begin(), ys.end());
        Point m{xs[xs.size() / 2], ys[ys.size() / 2]};
        return cheapestType(P, (int)g.size(), lenSum(m, g)) >= 0;
    }

    // Greedy nearest-neighbour grouping: seed = node farthest from SRC, then add
    // the closest remaining nodes while one buffer can still drive the group.
    void nnGroups(const std::vector<int>& ids, int limit, std::vector<std::vector<int>>& out) const {
        std::vector<int> rem = ids;
        while (!rem.empty()) {
            size_t si = 0;
            int sd = -1;
            for (size_t i = 0; i < rem.size(); ++i) {
                int d = manhattan(P.src, pts[rem[i]]);
                if (d > sd) { sd = d; si = i; }
            }
            int seed = rem[si];
            rem.erase(rem.begin() + si);
            std::sort(rem.begin(), rem.end(), [&](int a, int b) {
                return manhattan(pts[seed], pts[a]) < manhattan(pts[seed], pts[b]);
            });
            std::vector<int> g{seed};
            size_t taken = 0;
            while (taken < rem.size() && (int)g.size() < limit) {
                g.push_back(rem[taken]);
                if (!feasibleGroup(g)) { g.pop_back(); break; }
                ++taken;
            }
            rem.erase(rem.begin(), rem.begin() + taken);
            out.push_back(g);
        }
    }

    // Fallback when no grouping is possible: put a relay buffer in front of every
    // node, one hop toward SRC. Nodes converge toward SRC, where they can merge.
    bool advanceAll(std::vector<int>& cur) {
        bool any = false;
        for (size_t i = 0; i < cur.size(); ++i) {
            int s = cur[i];
            int d = manhattan(P.src, pts[s]);
            if (d <= 1) continue;
            int t = (int)std::min<long long>(maxL, d - 1);
            Point pos;
            if (!freeSpot(stepToward(pts[s], P.src, t), pos)) continue;
            int dKid = manhattan(pos, pts[s]);
            if (manhattan(P.src, pos) >= d) continue;
            int ty = cheapestType(P, 1, dKid);
            if (ty < 0) continue;
            cur[i] = create(pos, ty, {s});
            any = true;
        }
        return any;
    }

    bool build(Tree& T) {
        if (P.srcFanout < 1 || cap < 2) return false;
        std::vector<int> cur;
        for (int i = 1; i <= n; ++i) cur.push_back(i);
        for (int iter = 0; iter < 5000; ++iter) {
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
            if (mode == 1) nnGroups(cur, cap, groups);
            else bisect(cur, cap, groups);
            std::vector<int> nxt;
            for (auto& g : groups) makeGroup(g, nxt);
            if (nxt.size() >= cur.size()) {
                // no grouping possible: advance every node toward SRC, then retry
                if (!advanceAll(cur)) return false;
                continue;
            }
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
// Incremental simulated annealing
//
// The state is modified in place; each move touches only the affected parents
// (child count / total length / type) and the affected subtrees (arrival
// times). Sink arrival times live in a multiset, so Tmax / Tmin are O(1).
// A rejected move is rolled back from small journals. Removed buffers are only
// marked dead (ids are never renumbered); the tree is compacted on export.
// ---------------------------------------------------------------------------
typedef unsigned long long u64;
inline u64 keyOf(Point p) { return ((u64)(unsigned)p.x << 32) | (unsigned)p.y; }

struct RawTree {  // flat copy of the buffer slots, used for "best so far"
    std::vector<int> par;
    std::vector<Point> p;
    std::vector<int> typ;
    std::vector<char> alive;
};

struct IncState {
    struct Node {
        Point p;
        int par = -1;
        std::vector<int> ch;
        long long len = 0;
        long long arr = 0;
        int typ = -1;
        bool alive = true;
    };
    struct Snap {
        int id;
        Point p;
        int par;
        std::vector<int> ch;
        long long len;
        int typ;
        bool alive;
    };

    const Problem& P;
    int n = 0;
    std::vector<Node> nd;
    std::multiset<long long> sinkArr;
    long long cost = 0;
    std::unordered_set<u64> occ;
    std::vector<int> freeList;

    // per-move journals
    std::vector<Snap> snaps;
    std::vector<int> stamp;
    int curStamp = 0;
    std::vector<std::pair<int, long long>> arrJ;
    std::vector<int> pendingFree, stk, dirty;
    std::vector<Point> occAdd, occDel;
    long long costBefore = 0;
    int allocated = -1;

    explicit IncState(const Problem& pr) : P(pr), n((int)pr.sinks.size()) {}

    int d(int a, int b) const { return manhattan(nd[a].p, nd[b].p); }

    long long lenOf(int t) const {
        long long s = 0;
        for (int c : nd[t].ch) s += d(t, c);
        return s;
    }

    bool init(const Tree& T) {
        const int m = (int)T.bufs.size();
        const int N = 1 + n + m;
        nd.assign(N, Node());
        sinkArr.clear();
        occ.clear();
        freeList.clear();
        cost = 0;
        nd[0].p = P.src;
        nd[0].par = -1;
        for (int i = 1; i <= n; ++i) nd[i].p = P.sinks[i - 1];
        for (int k = 0; k < m; ++k) nd[n + 1 + k].p = T.bufs[k].p;
        for (int id = 1; id < N; ++id) {
            nd[id].par = T.parent[id];
            nd[T.parent[id]].ch.push_back(id);
        }
        for (int t = 0; t < N; ++t) {
            if (t >= 1 && t <= n) continue;
            nd[t].len = lenOf(t);
            if (t == 0) {
                if ((int)nd[0].ch.size() > P.srcFanout || nd[0].len > P.srcLength) return false;
                continue;
            }
            if (nd[t].ch.empty()) return false;
            int ty = cheapestType(P, (int)nd[t].ch.size(), nd[t].len);
            if (ty < 0) return false;
            nd[t].typ = ty;
            cost += P.lib[ty].cost;
        }
        std::vector<int> st{0};
        nd[0].arr = 0;
        while (!st.empty()) {
            int u = st.back();
            st.pop_back();
            for (int c : nd[u].ch) {
                nd[c].arr = nd[u].arr + d(u, c);
                st.push_back(c);
            }
        }
        for (int i = 1; i <= n; ++i) sinkArr.insert(nd[i].arr);
        for (int id = 0; id < N; ++id) occ.insert(keyOf(nd[id].p));
        stamp.assign(N, 0);
        curStamp = 0;
        return true;
    }

    long long score() const {
        return cost + (long long)P.wSkew * (*sinkArr.rbegin() - *sinkArr.begin());
    }

    // ---- journal helpers ----
    void beginMove() {
        ++curStamp;
        snaps.clear();
        arrJ.clear();
        pendingFree.clear();
        occAdd.clear();
        occDel.clear();
        costBefore = cost;
        allocated = -1;
    }
    void touch(int id) {
        if (stamp[id] == curStamp) return;
        stamp[id] = curStamp;
        const Node& x = nd[id];
        snaps.push_back({id, x.p, x.par, x.ch, x.len, x.typ, x.alive});
    }
    void setArr(int id, long long v) {
        arrJ.push_back({id, nd[id].arr});
        if (id >= 1 && id <= n) {
            sinkArr.erase(sinkArr.find(nd[id].arr));
            sinkArr.insert(v);
        }
        nd[id].arr = v;
    }
    void shiftSubtree(int root, long long delta) {
        if (delta == 0) return;
        stk.clear();
        stk.push_back(root);
        while (!stk.empty()) {
            int x = stk.back();
            stk.pop_back();
            setArr(x, nd[x].arr + delta);
            for (int c : nd[x].ch) stk.push_back(c);
        }
    }
    void commit() {
        for (int id : pendingFree) freeList.push_back(id);
        for (const Point& q : occDel) occ.erase(keyOf(q));
        for (const Point& q : occAdd) occ.insert(keyOf(q));
    }
    void undo() {
        for (int i = (int)arrJ.size() - 1; i >= 0; --i) {
            int id = arrJ[i].first;
            if (id >= 1 && id <= n) {
                sinkArr.erase(sinkArr.find(nd[id].arr));
                sinkArr.insert(arrJ[i].second);
            }
            nd[id].arr = arrJ[i].second;
        }
        for (Snap& s : snaps) {
            Node& x = nd[s.id];
            x.p = s.p; x.par = s.par; x.ch = std::move(s.ch);
            x.len = s.len; x.typ = s.typ; x.alive = s.alive;
        }
        cost = costBefore;
        if (allocated >= 0) freeList.push_back(allocated);
    }

    // ---- structure helpers ----
    void removeChild(int parent, int c) {
        auto& v = nd[parent].ch;
        for (size_t i = 0; i < v.size(); ++i)
            if (v[i] == c) { v[i] = v.back(); v.pop_back(); return; }
    }
    void setType(int t, int ty) {
        if (nd[t].typ >= 0) cost -= P.lib[nd[t].typ].cost;
        nd[t].typ = ty;
        if (ty >= 0) cost += P.lib[ty].cost;
    }
    bool retype(int t) {
        if (t == 0) return (int)nd[0].ch.size() <= P.srcFanout && nd[0].len <= P.srcLength;
        if (nd[t].ch.empty()) return false;
        int ty = cheapestType(P, (int)nd[t].ch.size(), nd[t].len);
        if (ty < 0) return false;
        setType(t, ty);
        return true;
    }
    void kill(int id) {
        setType(id, -1);
        nd[id].alive = false;
        nd[id].ch.clear();
        pendingFree.push_back(id);
        occDel.push_back(nd[id].p);
    }
    int allocBuffer() {
        int id;
        if (!freeList.empty()) { id = freeList.back(); freeList.pop_back(); }
        else { id = (int)nd.size(); nd.emplace_back(); nd[id].alive = false; stamp.resize(nd.size(), 0); }
        allocated = id;
        return id;
    }

    // ---- moves: each returns false if the result is illegal (caller undoes) ----
    bool movePos(int b, Point q) {
        beginMove();
        int p = nd[b].par;
        touch(b); touch(p);
        occDel.push_back(nd[b].p);
        occAdd.push_back(q);
        nd[b].p = q;
        nd[p].len = lenOf(p);
        nd[b].len = lenOf(b);
        if (!retype(p) || !retype(b)) return false;
        long long na = nd[p].arr + d(p, b);
        for (int c : nd[b].ch) shiftSubtree(c, na + d(b, c) - nd[c].arr);
        setArr(b, na);
        return true;
    }

    bool reassign(int x, int q) {
        beginMove();
        int p = nd[x].par;
        touch(x); touch(p); touch(q);
        removeChild(p, x);
        nd[q].ch.push_back(x);
        nd[x].par = q;
        nd[p].len = lenOf(p);
        nd[q].len = lenOf(q);
        dirty.clear();
        dirty.push_back(q);
        int cur = p;
        for (;;) {
            if (cur == 0 || !nd[cur].ch.empty()) { dirty.push_back(cur); break; }
            int gp = nd[cur].par;  // buffer became empty: delete it (cascade)
            touch(gp);
            removeChild(gp, cur);
            nd[gp].len = lenOf(gp);
            kill(cur);
            cur = gp;
        }
        for (int t : dirty)
            if (nd[t].alive && !retype(t)) return false;
        shiftSubtree(x, nd[q].arr + d(q, x) - nd[x].arr);
        return true;
    }

    bool swapSinks(int a, int b) {
        beginMove();
        int pa = nd[a].par, pb = nd[b].par;
        touch(a); touch(b); touch(pa); touch(pb);
        for (int& c : nd[pa].ch) if (c == a) { c = b; break; }
        for (int& c : nd[pb].ch) if (c == b) { c = a; break; }
        nd[a].par = pb;
        nd[b].par = pa;
        nd[pa].len = lenOf(pa);
        nd[pb].len = lenOf(pb);
        if (!retype(pa) || !retype(pb)) return false;
        setArr(a, nd[pb].arr + d(pb, a));
        setArr(b, nd[pa].arr + d(pa, b));
        return true;
    }

    bool insertBuf(int u, const std::vector<int>& kids, Point pos) {
        beginMove();
        int nb = allocBuffer();
        touch(nb); touch(u);
        for (int k : kids) touch(k);
        nd[nb].p = pos;
        nd[nb].par = u;
        nd[nb].ch = kids;
        nd[nb].alive = true;
        nd[nb].typ = -1;
        for (int k : kids) { removeChild(u, k); nd[k].par = nb; }
        nd[u].ch.push_back(nb);
        nd[nb].len = lenOf(nb);
        nd[u].len = lenOf(u);
        occAdd.push_back(pos);
        if (!retype(nb) || !retype(u)) return false;
        setArr(nb, nd[u].arr + d(u, nb));
        for (int k : kids) shiftSubtree(k, nd[nb].arr + d(nb, k) - nd[k].arr);
        return true;
    }

    bool removeBuf(int b) {
        beginMove();
        int p = nd[b].par;
        touch(b); touch(p);
        std::vector<int> kids = nd[b].ch;
        for (int c : kids) touch(c);
        removeChild(p, b);
        for (int c : kids) { nd[c].par = p; nd[p].ch.push_back(c); }
        nd[p].len = lenOf(p);
        kill(b);
        if (!retype(p)) return false;
        for (int c : kids) shiftSubtree(c, nd[p].arr + d(p, c) - nd[c].arr);
        return true;
    }

    // ---- export ----
    void saveRaw(RawTree& R) const {
        size_t S = nd.size();
        R.par.resize(S); R.p.resize(S); R.typ.resize(S); R.alive.resize(S);
        for (size_t i = 0; i < S; ++i) {
            R.par[i] = nd[i].par; R.p[i] = nd[i].p; R.typ[i] = nd[i].typ; R.alive[i] = nd[i].alive;
        }
    }
    Tree toTree(const RawTree& R) const {
        const int S = (int)R.par.size();
        std::vector<int> nid(S, -1);
        int m = 0;
        for (int id = 1; id <= n; ++id) nid[id] = id;
        for (int id = n + 1; id < S; ++id) if (R.alive[id]) nid[id] = n + 1 + m++;
        Tree T;
        T.bufs.resize(m);
        T.parent.assign(1 + n + m, -1);
        for (int id = 1; id < S; ++id) {
            if (id > n && !R.alive[id]) continue;
            int ni = nid[id];
            T.parent[ni] = R.par[id] == 0 ? 0 : nid[R.par[id]];
            if (id > n) T.bufs[ni - n - 1] = {R.typ[id], R.p[id]};
        }
        return T;
    }
};

void annealing(const Problem& P, Tree& best, long long& bestScore,
               const Clock::time_point& startT, const Clock::time_point& endT) {
    const int n = (int)P.sinks.size();
    if (n < 2) return;
    std::mt19937_64 rng(12345);
    auto rnd = [&](int lo, int hi) { return lo + (int)(rng() % (u64)(hi - lo + 1)); };
    auto unif = [&]() { return (double)(rng() >> 11) / 9007199254740992.0; };
    const bool dbg = std::getenv("CBI_DEBUG") != nullptr;

    IncState S(P);
    if (!S.init(best)) return;
    long long curScore = S.score();
    bestScore = curScore;
    RawTree bestRaw;
    S.saveRaw(bestRaw);

    const int maxDim = std::max(P.dimX, P.dimY);
    double prog = 0.0;
    auto inChip = [&](Point q) { return q.x >= 0 && q.x <= P.dimX && q.y >= 0 && q.y <= P.dimY; };

    auto pickBuf = [&]() -> int {
        int slots = (int)S.nd.size();
        if (slots <= n + 1) return -1;
        for (int t = 0; t < 8; ++t) {
            int id = rnd(n + 1, slots - 1);
            if (S.nd[id].alive) return id;
        }
        return -1;
    };
    auto pickParent = [&]() -> int {  // SRC or an alive buffer
        int slots = (int)S.nd.size();
        for (int t = 0; t < 8; ++t) {
            int id = rnd(n, slots - 1);
            if (id == n) return 0;
            if (S.nd[id].alive) return id;
        }
        return 0;
    };

    // -1: no move generated, 0: illegal (undo needed), 1: legal and applied
    auto tryMove = [&]() -> int {
        const int slots = (int)S.nd.size();
        int r = rnd(0, 99);
        if (r < 40) {  // move a buffer
            int b = pickBuf();
            if (b < 0) return -1;
            Point o = S.nd[b].p;
            int maxStep = std::max(2, (int)(maxDim / 6.0 * (1.0 - prog)));
            int s = (rnd(0, 1) ? rnd(1, 2) : rnd(1, maxStep));
            Point q{o.x + rnd(-s, s), o.y + rnd(-s, s)};
            if ((q.x == o.x && q.y == o.y) || !inChip(q) || S.occ.count(keyOf(q))) return -1;
            return S.movePos(b, q) ? 1 : 0;
        }
        if (r < 65) {  // reassign node x to a nearby parent
            int x = -1;
            for (int t = 0; t < 8 && x < 0; ++t) {
                int id = rnd(1, slots - 1);
                if (id <= n || S.nd[id].alive) x = id;
            }
            if (x < 0) return -1;
            int bq = -1, bd = 1 << 30;
            for (int t = 0; t < 6; ++t) {
                int q = pickParent();
                if (q == x || q == S.nd[x].par) continue;
                if (x > n) {
                    bool bad = false;
                    for (int y = q; y > 0; y = S.nd[y].par) if (y == x) { bad = true; break; }
                    if (bad) continue;
                }
                int dd = S.d(q, x);
                if (dd < bd) { bd = dd; bq = q; }
            }
            if (bq < 0) return -1;
            return S.reassign(x, bq) ? 1 : 0;
        }
        if (r < 80) {  // swap the parents of two sinks
            int a = rnd(1, n), bb = -1, bd = 1 << 30;
            for (int t = 0; t < 6; ++t) {
                int b = rnd(1, n);
                if (b == a || S.nd[b].par == S.nd[a].par) continue;
                int dd = S.d(a, b);
                if (dd < bd) { bd = dd; bb = b; }
            }
            if (bb < 0) return -1;
            return S.swapSinks(a, bb) ? 1 : 0;
        }
        if (r < 90) {  // insert a buffer above 2-3 siblings
            int u = pickParent();
            std::vector<int> kids = S.nd[u].ch;
            if (kids.size() < 2) return -1;
            int a = kids[rnd(0, (int)kids.size() - 1)];
            std::sort(kids.begin(), kids.end(), [&](int x, int y) { return S.d(a, x) < S.d(a, y); });
            int g = std::min<int>((int)kids.size(), rnd(2, 3));
            kids.resize(g);
            std::vector<int> xs, ys;
            for (int k : kids) { xs.push_back(S.nd[k].p.x); ys.push_back(S.nd[k].p.y); }
            std::sort(xs.begin(), xs.end());
            std::sort(ys.begin(), ys.end());
            Point want{xs[g / 2], ys[g / 2]}, pos;
            bool found = false;
            for (int rr = 0; rr <= 4 && !found; ++rr)
                for (int dx = -rr; dx <= rr && !found; ++dx) {
                    int rem = rr - std::abs(dx);
                    for (int sg = 0; sg < 2 && !found; ++sg) {
                        if (sg == 1 && rem == 0) continue;
                        Point c{want.x + dx, want.y + (sg ? -rem : rem)};
                        if (inChip(c) && !S.occ.count(keyOf(c))) { pos = c; found = true; }
                    }
                }
            if (!found) return -1;
            return S.insertBuf(u, kids, pos) ? 1 : 0;
        }
        int b = pickBuf();  // remove a buffer
        if (b < 0) return -1;
        return S.removeBuf(b) ? 1 : 0;
    };

    // Calibrate the starting temperature from typical |delta| of legal moves.
    double sum = 0; int cntD = 0;
    for (int tries = 0; tries < 3000 && cntD < 300; ++tries) {
        int k = tryMove();
        if (k < 0) continue;
        if (k == 1) {
            long long d = S.score() - curScore;
            if (d != 0) { sum += (double)std::llabs(d); ++cntD; }
        }
        S.undo();
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
        int k = tryMove();
        if (k < 0) continue;
        if (k == 0) { S.undo(); continue; }
        long long sc = S.score();
        long long d = sc - curScore;
        if (d <= 0 || unif() < std::exp(-(double)d / temp)) {
            S.commit();
            curScore = sc;
            if (curScore < bestScore) { bestScore = curScore; S.saveRaw(bestRaw); sinceBest = 0; }
            if (dbg && (it % 997) == 0) {  // cross-check against the full evaluator
                RawTree R; S.saveRaw(R);
                EvalResult e = evaluate(P, S.toTree(R));
                if (!e.legal || e.score != curScore) {
                    std::fprintf(stderr, "DEBUG MISMATCH at iter %lld: incremental=%lld full=%lld legal=%d (%s)\n",
                                 it, curScore, e.score, (int)e.legal, e.msg.c_str());
                    std::abort();
                }
            }
        } else {
            S.undo();
        }
        if (++sinceBest > 400000 && prog > 0.5) {  // late phase: restart from the best
            Tree bt = S.toTree(bestRaw);
            if (S.init(bt)) { curScore = S.score(); }
            sinceBest = 0;
        }
    }
    best = S.toTree(bestRaw);
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

    // Candidates: clustering with different group-size caps (two grouping modes).
    std::set<int> caps;
    int maxF = 0;
    for (auto& t : P.lib) { caps.insert(t.fanout); maxF = std::max(maxF, t.fanout); }
    for (int c = 2; c <= std::min(maxF, 16); ++c) caps.insert(c);
    const auto solveStart = Clock::now();
    auto overBudget = [&](double f) {
        return Clock::now() > solveStart + std::chrono::milliseconds((long long)(timeLimitSec * f * 1000));
    };
    for (int c : caps) {
        if (c < 2) continue;
        if (have && overBudget(0.25)) break;  // keep stage 1 cheap on huge inputs
        Builder b(P, c);
        Tree T;
        if (b.build(T)) consider(T, "candidate: bisect cap=" + std::to_string(c));
    }
    // Nearest-neighbour grouping is O(N^2): use it on small/medium inputs, or as a
    // rescue when bisection found nothing legal.
    if (n <= 1500 || !have) {
        std::set<int> nnCaps;
        for (auto& t : P.lib) if (t.fanout >= 2) nnCaps.insert(t.fanout);
        for (int c : nnCaps) {
            if (have && overBudget(0.30)) break;
            Builder b(P, c);
            b.mode = 1;
            Tree T;
            if (b.build(T)) consider(T, "candidate: nn-greedy cap=" + std::to_string(c));
        }
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
        const auto dl = at(n <= 2000 ? 0.25 : 0.03);
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
        while (n <= 2000 && improved && !timeUp(deadline)) {
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
