#include "evaluator.h"

#include <set>
#include <utility>

std::string nodeName(int n, int id) {
    if (id == 0) return "SRC";
    if (id <= n) return "S" + std::to_string(id);
    return "B" + std::to_string(id - n);
}

Point nodePoint(const Problem& P, const Tree& T, int id) {
    int n = (int)P.sinks.size();
    if (id == 0) return P.src;
    if (id <= n) return P.sinks[id - 1];
    return T.bufs[id - n - 1].p;
}

EvalResult evaluate(const Problem& P, const Tree& T) {
    EvalResult R;
    const int n = (int)P.sinks.size();
    const int m = (int)T.bufs.size();
    const int N = 1 + n + m;
    auto fail = [&](const std::string& s) {
        R.legal = false;
        R.msg = s;
        return R;
    };

    if ((int)T.parent.size() != N) return fail("parent array size mismatch");

    for (int k = 0; k < m; ++k) {
        const Buffer& b = T.bufs[k];
        std::string nm = nodeName(n, n + 1 + k);
        if (b.type < 0 || b.type >= (int)P.lib.size()) return fail(nm + ": bad type");
        if (b.p.x < 0 || b.p.x > P.dimX || b.p.y < 0 || b.p.y > P.dimY)
            return fail(nm + ": outside chip");
        R.cost += P.lib[b.type].cost;
    }

    std::set<std::pair<int, int>> seen;
    for (int id = 0; id < N; ++id) {
        Point p = nodePoint(P, T, id);
        if (!seen.insert({p.x, p.y}).second)
            return fail(nodeName(n, id) + ": duplicate coordinate");
    }

    std::vector<std::vector<int>> ch(N);
    for (int id = 1; id < N; ++id) {
        int par = T.parent[id];
        if (par < 0 || par >= N || par == id) return fail(nodeName(n, id) + ": bad parent");
        if (par >= 1 && par <= n) return fail(nodeName(n, id) + ": parent is a sink");
        ch[par].push_back(id);
    }

    for (int u = 0; u < N; ++u) {
        if (u >= 1 && u <= n) continue;
        int F;
        long long L;
        if (u == 0) { F = P.srcFanout; L = P.srcLength; }
        else { F = P.lib[T.bufs[u - n - 1].type].fanout; L = P.lib[T.bufs[u - n - 1].type].length; }
        if ((int)ch[u].size() > F) return fail(nodeName(n, u) + ": fanout exceeded");
        long long sum = 0;
        Point pu = nodePoint(P, T, u);
        for (int c : ch[u]) sum += manhattan(pu, nodePoint(P, T, c));
        if (sum > L) return fail(nodeName(n, u) + ": length exceeded");
        if (u > n && ch[u].empty()) return fail(nodeName(n, u) + ": unused buffer");
    }

    std::vector<long long> arr(N, 0);
    std::vector<int> st{0};
    int visited = 0;
    while (!st.empty()) {
        int u = st.back();
        st.pop_back();
        ++visited;
        Point pu = nodePoint(P, T, u);
        for (int c : ch[u]) {
            arr[c] = arr[u] + manhattan(pu, nodePoint(P, T, c));
            st.push_back(c);
        }
    }
    if (visited != N) return fail("not a single tree (cycle or unreachable node)");

    if (n > 0) {
        R.tmax = R.tmin = arr[1];
        for (int i = 1; i <= n; ++i) {
            if (arr[i] > R.tmax) R.tmax = arr[i];
            if (arr[i] < R.tmin) R.tmin = arr[i];
        }
    }
    R.score = R.cost + (long long)P.wSkew * (R.tmax - R.tmin);
    R.legal = true;
    return R;
}
