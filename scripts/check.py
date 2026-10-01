#!/usr/bin/env python3
"""Independent legality checker: check.py input.cbi output.cbi -> prints T_max, T_min, Score."""
import re, sys

def toks(path):
    out = []
    for line in open(path):
        out += line.split('#')[0].split()
    return out

def parse_in(path):
    t = toks(path); i = 0; P = {'lib': {}}
    while i < len(t):
        k = t[i]; i += 1
        if k == '.limit':
            while t[i] != '.e':
                if t[i] == 'fanout': P['F0'] = int(t[i+1]); i += 2
                elif t[i] == 'length': P['L0'] = int(t[i+1]); i += 2
                elif t[i] == '.dimx': P['dx'] = int(t[i+1]); i += 2
                elif t[i] == '.dimy': P['dy'] = int(t[i+1]); i += 2
                else: i += 1
            i += 1
        elif k == '.buflib':
            n = int(t[i]); i += 1
            for _ in range(n):
                P['lib'][t[i]] = tuple(int(x) for x in t[i+1:i+4]); i += 4
            i += 1
        elif k == '.objective':
            P['w'] = int(t[i+1]); i += 3
        elif k == '.pin':
            n = int(t[i]); i += 1
            pts = [(int(t[i+2*j]), int(t[i+2*j+1])) for j in range(n)]
            i += 2*n + 1
            P['src'] = pts[0]; P['sinks'] = pts[1:]
    return P

def main(inp, outp):
    P = parse_in(inp); n = len(P['sinks'])
    pos = {'SRC': P['src']}; typ = {}
    for j, s in enumerate(P['sinks']): pos['S%d' % (j+1)] = s
    txt = open(outp).read()
    bsec = txt.split('.buffer')[1].split('.e')[0].strip().splitlines()
    m = int(bsec[0].split()[0]); rows = bsec[1:]
    assert len(rows) == m, 'buffer count mismatch'
    for j, r in enumerate(rows):
        f = r.split('#')[0].split()
        assert f[0] == 'B%d' % (j+1), 'buffer ids must be consecutive'
        assert f[1] in P['lib'], 'unknown type'
        pos[f[0]] = (int(f[2]), int(f[3])); typ[f[0]] = f[1]
        assert 0 <= pos[f[0]][0] <= P['dx'] and 0 <= pos[f[0]][1] <= P['dy'], 'outside chip'
    assert len(set(pos.values())) == len(pos), 'duplicate coordinates'
    lsec = txt.split('.level')[1].split('.e')[0].strip().splitlines()
    K = int(lsec[0].split()[0]); par = {}; lvl_parents = {}
    for line in lsec[1:]:
        line = line.split('#')[0].strip()
        k, rest = line.split(None, 1)
        for pm in re.finditer(r'(\w+):\{([^}]*)\}', rest):
            p = pm.group(1); kids = pm.group(2).split()
            F, L = (P['F0'], P['L0']) if p == 'SRC' else P['lib'][typ[p]][:2]
            assert len(kids) <= F, p + ' fanout'
            d = lambda a, b: abs(pos[a][0]-pos[b][0]) + abs(pos[a][1]-pos[b][1])
            assert sum(d(p, c) for c in kids) <= L, p + ' length'
            for c in kids:
                assert c not in par, c + ' has 2 parents'
                par[c] = p
            lvl_parents.setdefault(int(k), []).append(p)
    assert max(lvl_parents) == K, '.level value wrong'
    for name in pos:
        if name != 'SRC': assert name in par, name + ' unreachable'
    arr = {}
    def T(x):
        if x == 'SRC': return 0
        if x not in arr:
            p = par[x]; arr[x] = T(p) + abs(pos[p][0]-pos[x][0]) + abs(pos[p][1]-pos[x][1])
        return arr[x]
    sys.setrecursionlimit(100000)
    ts = [T('S%d' % (j+1)) for j in range(n)]
    cost = sum(P['lib'][typ[b]][2] for b in typ)
    print('T_max: %d, T_min: %d, Score: %d' % (max(ts), min(ts), cost + P['w']*(max(ts)-min(ts))))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
