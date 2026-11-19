#!/usr/bin/env python3
"""results/raw/<exp>/*.json -> results/processed/runs.csv (one row per run)
                              + results/processed/summary.csv (mean/std over repetitions).
Usage: parse.py [results_dir]

Knobs come from the file name (rw=...,bs=...,qd=...), not from fio's output.
Latencies are in microseconds, bandwidth in MB/s (1e6 B/s).
Unprefixed columns describe the whole run; r_/w_ prefixes are per direction. The
unprefixed pXX columns are those of the dominant direction (more IOPS).
"""
import csv, glob, json, os, sys, warnings
from collections import defaultdict
import numpy as np

RES = sys.argv[1] if len(sys.argv) > 1 else "results"
PCT = [1, 5, 10, 25, 50, 75, 90, 95, 99, 99.5, 99.9, 99.95, 99.99, 99.999]  # = percentile_list in bench.fio
KEYS = ["exp", "rw", "bs", "bs_KiB", "qd", "mix", "range", "direct", "engine", "tag"]
warnings.simplefilter("ignore", RuntimeWarning)


def percentiles(d):
    for k in ("lat_ns", "clat_ns"):  # total latency if present, else completion latency
        p = d.get(k, {}).get("percentile")
        if p:
            return {float(a): v / 1e3 for a, v in p.items()}
    return {}


def side(d, pre):
    p = percentiles(d)
    bw = d["bw_bytes"] / 1e6 if "bw_bytes" in d else d["bw"] * 1024 / 1e6
    o = {pre + "iops": d["iops"], pre + "bw_MBs": bw, pre + "lat_mean_us": d["lat_ns"]["mean"] / 1e3}
    for x in PCT:
        o[f"{pre}p{x:g}"] = p.get(float(x), float("nan"))
    return o


rows = []
for f in sorted(glob.glob(f"{RES}/raw/*/rw=*.json")):
    txt = open(f).read()
    job = json.loads(txt[txt.index("{"):])["jobs"][0]  # skip any text fio printed before the JSON
    kv = dict(p.split("=", 1) for p in os.path.basename(f)[:-5].split(","))
    bs = kv["bs"]
    r = {"exp": os.path.basename(os.path.dirname(f))}
    r.update({k: kv[k] for k in ("rw", "bs", "qd", "mix", "range", "direct", "engine", "tag")})
    r["bs_KiB"] = int(bs[:-1]) * (1024 if bs[-1] == "m" else 1)
    r["rep"] = int(kv["rep"])
    R, W = side(job["read"], "r_"), side(job["write"], "w_")
    r.update(R); r.update(W)
    r["iops"] = R["r_iops"] + W["w_iops"]
    r["bw_MBs"] = R["r_bw_MBs"] + W["w_bw_MBs"]
    # IOPS-weighted mean latency, so that iops * lat_mean = outstanding IOs (Little's Law)
    r["lat_mean_us"] = (R["r_iops"] * R["r_lat_mean_us"] + W["w_iops"] * W["w_lat_mean_us"]) / r["iops"]
    main = R if R["r_iops"] >= W["w_iops"] else W
    pre = "r_" if main is R else "w_"
    for x in PCT:
        r[f"p{x:g}"] = main[f"{pre}p{x:g}"]
    r["usr_cpu"], r["sys_cpu"] = job["usr_cpu"], job["sys_cpu"]
    try:
        r["temp_before_C"], r["temp_after_C"] = map(float, open(f[:-5] + ".temp").read().split())
    except (OSError, ValueError):
        r["temp_before_C"] = r["temp_after_C"] = float("nan")
    rows.append(r)

if not rows:
    sys.exit(f"no runs found in {RES}/raw")

os.makedirs(f"{RES}/processed", exist_ok=True)
with open(f"{RES}/processed/runs.csv", "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0]), lineterminator="\n")
    w.writeheader(); w.writerows(rows)

groups = defaultdict(list)
for r in rows:
    groups[tuple(r[k] for k in KEYS)].append(r)
metrics = [k for k in rows[0] if k not in KEYS and k != "rep"]
with open(f"{RES}/processed/summary.csv", "w", newline="") as fh:
    w = csv.writer(fh, lineterminator="\n")
    w.writerow(KEYS + ["n"] + [m + s for m in metrics for s in ("_mean", "_std")])
    for key, g in groups.items():
        out = list(key) + [len(g)]
        for m in metrics:
            v = np.array([r[m] for r in g], dtype=float)
            out += [np.nanmean(v), np.std(v, ddof=1) if len(v) > 1 else 0.0]
        w.writerow(out)
print(f"{len(rows)} runs -> {len(groups)} data points in {RES}/processed/")
