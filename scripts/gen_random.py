#!/usr/bin/env python3
"""gen_random.py N SEED W_SKEW > input.cbi   (random test generator)"""
import random, sys
n, seed, w = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
random.seed(seed)
dx = dy = max(100, int((n * 40) ** 0.5 * 4))
pts = set()
while len(pts) < n + 1:
    pts.add((random.randint(0, dx), random.randint(0, dy)))
pts = list(pts)
print(".limit\nfanout %d\nlength %d\n.dimx %d\n.dimy %d\n.e\n" % (random.randint(2, 5), random.randint(dx, 3*dx), dx, dy))
print(".buflib 3\nSMALL 3 %d 10\nMEDIUM 6 %d 16\nLARGE 9 %d 22\n.e\n" % (dx, 2*dx, 4*dx))
print(".objective\nw_skew %d\n.e\n" % w)
print(".pin %d" % (n + 1))
for p in pts: print(*p)
print(".e")
