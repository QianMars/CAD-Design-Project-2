#!/usr/bin/env python3
"""plot_results.py -> results/fig_wsweep.png from results/experiments.json"""
import json, os
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
R = json.load(open(os.path.join(os.path.dirname(__file__), "..", "results", "experiments.json")))
ws = R["wsweep"]; w = [r["w"] for r in ws]
fig, ax = plt.subplots(1, 2, figsize=(7.2, 2.6))
ax[0].plot(w, [r["after_final"]["cost"] for r in ws], "s-", label="buffer cost", color="#d55e00")
ax[0].set_xscale("log"); ax[0].set_xlabel("w_skew"); ax[0].set_ylabel("buffer cost", color="#d55e00")
a2 = ax[0].twinx(); a2.plot(w, [r["after_final"]["skew"] for r in ws], "o--", label="skew", color="#0072b2")
a2.set_ylabel("skew", color="#0072b2"); ax[0].set_title("Trade-off vs. w_skew (n=100)", fontsize=9)
ab = R.get("ablation", {})
if ab:
    names = list(ab); means = [sum(v)/len(v) for v in ab.values()]
    ax[1].barh(names[::-1], means[::-1], color=["#009e73" if n == "full" else "#999999" for n in names[::-1]])
    ax[1].set_xlabel("mean final Score (3 seeds, n=200)"); ax[1].set_title("Ablation", fontsize=9)
    ax[1].tick_params(labelsize=7)
plt.tight_layout(); plt.savefig(os.path.join(os.path.dirname(__file__), "..", "results", "fig_wsweep.png"), dpi=200); print("saved")
