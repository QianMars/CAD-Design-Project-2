#!/usr/bin/env python3
"""run_experiments.py [scale|wsweep|extreme|ablation|all]  -> results/experiments.json
Runs ./build/cbi on generated inputs, parses CBI_VERBOSE stage lines, verifies every
output with scripts/check.py, and stores everything as JSON (used for the report)."""
import json, os, re, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CBI = os.path.join(ROOT, "build", "cbi")
TMP = "/tmp/cbi_exp"; os.makedirs(TMP, exist_ok=True)
os.makedirs(os.path.join(ROOT, "results"), exist_ok=True)
STAGE = re.compile(r"^(initial|after greedy|after simulated|after final)[^\n]*?cost=(\d+)\s+skew=(\d+)\s+score=(\d+)", re.M)

def gen(case, n, seed, w):
    path = f"{TMP}/{case}_{n}_{seed}_{w}.cbi"
    with open(path, "w") as f:
        subprocess.run([sys.executable, f"{ROOT}/scripts/gen_extreme.py", case, str(n), str(seed), str(w)], stdout=f, check=True)
    return path

def run(inp, env_extra=None, tl=None):
    out = inp.replace(".cbi", ".out")
    env = dict(os.environ, CBI_VERBOSE="1"); env.pop("CBI_TIME", None)
    if tl: env["CBI_TIME"] = str(tl)
    env.update(env_extra or {})
    t0 = time.time()
    r = subprocess.run([CBI, inp, out], env=env, capture_output=True, text=True)
    dt = time.time() - t0
    res = {"time": round(dt, 1), "rc": r.returncode}
    for name, c, s, sc in STAGE.findall(r.stderr):
        res[name.split()[0] + ("_" + name.split()[1] if " " in name else "")] = {"cost": int(c), "skew": int(s), "score": int(sc)}
    m = re.search(r"T_max: (\d+), T_min: (\d+), Score: (\d+)", r.stdout)
    chk = subprocess.run([sys.executable, f"{ROOT}/scripts/check.py", inp, out], capture_output=True, text=True)
    res["legal"] = bool(m) and chk.returncode == 0 and chk.stdout.strip() == r.stdout.strip()
    if m: res["final"] = int(m.group(3))
    return res

results = json.load(open(f"{ROOT}/results/experiments.json")) if os.path.exists(f"{ROOT}/results/experiments.json") else {}
def save(): json.dump(results, open(f"{ROOT}/results/experiments.json", "w"), indent=1)
mode = sys.argv[1] if len(sys.argv) > 1 else "all"

if mode in ("scale", "all"):
    results.setdefault("scale", [])
    for n in (20, 100, 500, 2000, 10000):
        if any(x["n"] == n for x in results["scale"]): continue   # resume support
        r = run(gen("normal", n, 1, 1)); r.update(n=n, w=1); results["scale"].append(r); save(); print("scale", n, r, flush=True)
if mode in ("wsweep", "all"):
    results["wsweep"] = []
    for w in (1, 3, 10, 30, 100):
        r = run(gen("normal", 100, 1, w)); r.update(n=100, w=w); results["wsweep"].append(r); save(); print("wsweep", w, r, flush=True)
if mode in ("extreme", "all"):
    results["extreme"] = []
    for case, n in (("tinysrc", 60), ("srcf1", 60), ("shortlib", 60), ("dense", 100), ("normal", 200), ("big", 5000)):
        r = run(gen(case, n, 1, 1)); r.update(case=case, n=n); results["extreme"].append(r); save(); print("extreme", case, r, flush=True)
if mode in ("ablation", "all"):
    variants = {"full": {}, "no SA (greedy only)": {"CBI_NOSA": "1"}, "no move-buffer": {"CBI_NOMOVE": "pos"},
                "no reassign": {"CBI_NOMOVE": "reassign"}, "no swap": {"CBI_NOMOVE": "swap"},
                "no insert": {"CBI_NOMOVE": "insert"}, "no remove": {"CBI_NOMOVE": "remove"}}
    results["ablation"] = {}
    for name, env in variants.items():
        scores = []
        for seed in (1, 2, 3):
            r = run(gen("normal", 200, seed, 1), env, tl=8); scores.append(r["final"] if r["legal"] else None)
        results["ablation"][name] = scores; save(); print("ablation", name, scores, flush=True)
print("DONE", flush=True)
