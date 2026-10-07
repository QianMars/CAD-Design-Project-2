#!/usr/bin/env python3
"""plot_tree.py INPUT.cbi OUT1.cbi [OUT2.cbi ...] -o fig.png [-t "title1" "title2" ...] [--edges]
Draws one or more clock solutions side by side: points + labels only (like the project handout);
buffer type = marker colour. Use --edges to also draw the Manhattan parent->child wires."""
import argparse, re, sys
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

def toks(path):
    t = []
    for line in open(path): t += line.split('#')[0].split()
    return t

def read_input(path):
    t = toks(path); i = 0; P = {'lib': {}}
    while i < len(t):
        k = t[i]; i += 1
        if k == '.limit':
            while t[i] != '.e':
                if t[i] == '.dimx': P['dx'] = int(t[i+1]); i += 2
                elif t[i] == '.dimy': P['dy'] = int(t[i+1]); i += 2
                else: i += 1
            i += 1
        elif k == '.pin':
            n = int(t[i]); i += 1
            pts = [(int(t[i+2*j]), int(t[i+2*j+1])) for j in range(n)]; i += 2*n + 1
            P['src'], P['sinks'] = pts[0], pts[1:]
    return P

def read_output(path, P):
    pos = {'SRC': P['src']}; typ = {}
    for j, s in enumerate(P['sinks']): pos['S%d' % (j+1)] = s
    txt = open(path).read()
    for r in txt.split('.buffer')[1].split('.e')[0].strip().splitlines()[1:]:
        f = r.split('#')[0].split(); pos[f[0]] = (int(f[2]), int(f[3])); typ[f[0]] = f[1]
    edges = []
    for line in txt.split('.level')[1].split('.e')[0].strip().splitlines()[1:]:
        for pm in re.finditer(r'(\w+):\{([^}]*)\}', line):
            edges += [(pm.group(1), c) for c in pm.group(2).split()]
    return pos, typ, edges

def arrival(pos, edges):
    par = {c: p for p, c in edges}; memo = {'SRC': 0}
    def T(x):
        if x not in memo:
            p = par[x]; memo[x] = T(p) + abs(pos[p][0]-pos[x][0]) + abs(pos[p][1]-pos[x][1])
        return memo[x]
    return T

ap = argparse.ArgumentParser()
ap.add_argument('inp'); ap.add_argument('outs', nargs='+'); ap.add_argument('-o', default='tree.png')
ap.add_argument('-t', nargs='*', default=None)
ap.add_argument('--edges', action='store_true', help='also draw Manhattan wires')
a = ap.parse_args()
P = read_input(a.inp)
colors = {'SMALL': '#56b4e9', 'MEDIUM': '#e69f00', 'LARGE': '#f0e442'}   # same scheme as the handout
fig, axes = plt.subplots(1, len(a.outs), figsize=(3.6 * len(a.outs), 3.9), squeeze=False)
for ax, path, title in zip(axes[0], a.outs, a.t or [''] * len(a.outs)):
    pos, typ, edges = read_output(path, P)
    if a.edges:
        for p, c in edges:   # Manhattan route: horizontal first, then vertical
            (x0, y0), (x1, y1) = pos[p], pos[c]
            ax.plot([x0, x1, x1], [y0, y0, y1], color='#bbbbbb', lw=0.8, zorder=1)
    T = arrival(pos, edges); sk = [T('S%d' % (j+1)) for j in range(len(P['sinks']))]
    for name, (x, y) in pos.items():
        if name == 'SRC': ax.scatter(x, y, marker='^', s=70, c='#9400d3', zorder=3)
        elif name.startswith('S'): ax.scatter(x, y, marker='o', s=30, c='#009e73', zorder=3)
        else: ax.scatter(x, y, marker='s', s=55, c=colors.get(typ[name], 'k'), edgecolors='#444444', linewidths=0.4, zorder=3)
    # labels (red, like the handout); greedy placement that avoids other labels / points
    placed = []
    pts_all = list(pos.values())
    for name, (x, y) in pos.items():
        w_, h_ = 3.2 * len(name) + 1, 6.0
        for dx, dy in [(1.8, 1.5), (1.8, -6.5), (-w_ - 1.8, 1.5), (-w_ - 1.8, -6.5), (1.8, 6.0), (-w_ - 1.8, 6.0), (1.8, -11.5)]:
            bx, by = x + dx, y + dy
            if any(bx < ox + ow and ox < bx + w_ and by < oy + oh and oy < by + h_ for ox, oy, ow, oh in placed): continue
            if any(bx - 1 < px < bx + w_ + 1 and by - 1 < py < by + h_ + 1 for px, py in pts_all if (px, py) != (x, y)): continue
            break
        placed.append((bx, by, w_, h_))
        ax.text(bx, by, name, color='red', fontsize=7, va='bottom', ha='left')
    ax.set_xlim(-3, P['dx'] + 3); ax.set_ylim(-3, P['dy'] + 3); ax.set_aspect('equal'); ax.tick_params(labelsize=6)
    ax.set_title('%s\nbuffers=%d  skew=%d' % (title, len(typ), max(sk) - min(sk)), fontsize=8)
from matplotlib.lines import Line2D
handles = [Line2D([0], [0], marker='^', color='w', markerfacecolor='#9400d3', markersize=8, label='SRC'),
           Line2D([0], [0], marker='o', color='w', markerfacecolor='#009e73', markersize=6, label='sink')]
used = sorted({t for tt in [read_output(o, P)[1] for o in a.outs] for t in tt.values()}, key=lambda k: ['SMALL', 'MEDIUM', 'LARGE'].index(k) if k in ('SMALL', 'MEDIUM', 'LARGE') else 9)
handles += [Line2D([0], [0], marker='s', color='w', markerfacecolor=colors.get(t, 'k'), markeredgecolor='#444444', markersize=7, label=t) for t in used]
fig.legend(handles=handles, loc='lower center', ncol=len(handles), fontsize=7, frameon=False)
plt.tight_layout(rect=(0, 0.06, 1, 1)); plt.savefig(a.o, dpi=200); print('saved', a.o)
