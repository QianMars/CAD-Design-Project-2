#!/usr/bin/env python3
"""gen_extreme.py CASE N SEED [W_SKEW] > input.cbi   (corner-case generator for stress tests)
CASE: tinysrc | srcf1 | shortlib | dense | normal | big
"""
import random, sys
case, n, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
w = int(sys.argv[4]) if len(sys.argv) > 4 else 1
random.seed(seed)
lib = [("SMALL", 3, 90, 10), ("MEDIUM", 6, 120, 16), ("LARGE", 9, 190, 22)]
F0, L0, dim = 2, 100, 100
src = None
if case == "tinysrc":      # SRC length budget is tiny and SRC is far from the sinks
    dim, F0, L0 = 200, 3, 20; src = (0, 0)
elif case == "srcf1":      # SRC can drive exactly one child
    dim, F0, L0 = 150, 1, 450
elif case == "shortlib":   # buffers can only reach very short distances
    dim, F0, L0 = 300, 4, 600
    lib = [("SMALL", 3, 40, 10), ("MEDIUM", 6, 60, 16), ("LARGE", 9, 80, 22)]
elif case == "dense":      # almost every grid point is occupied
    dim, F0, L0 = 12, 3, 100
elif case == "big":
    dim = max(100, int((n * 40) ** 0.5 * 4)); F0, L0 = 5, 3 * dim
    lib = [("SMALL", 3, dim, 10), ("MEDIUM", 6, 2 * dim, 16), ("LARGE", 9, 4 * dim, 22)]
else:
    dim = max(100, int((n * 40) ** 0.5 * 4)); F0, L0 = 3, 2 * dim
    lib = [("SMALL", 3, dim, 10), ("MEDIUM", 6, 2 * dim, 16), ("LARGE", 9, 4 * dim, 22)]
pts = set()
if src: pts.add(src)
while len(pts) < n + 1:
    if case == "tinysrc":
        pts.add((random.randint(dim // 2, dim), random.randint(dim // 2, dim)))
    else:
        pts.add((random.randint(0, dim), random.randint(0, dim)))
pts = list(pts)
if src: pts.remove(src); pts = [src] + pts
print(".limit\nfanout %d\nlength %d\n.dimx %d\n.dimy %d\n.e\n" % (F0, L0, dim, dim))
print(".buflib %d" % len(lib))
for t in lib: print(*t)
print(".e\n\n.objective\nw_skew %d\n.e\n" % w)
print(".pin %d" % (n + 1))
for p in pts: print(*p)
print(".e")
