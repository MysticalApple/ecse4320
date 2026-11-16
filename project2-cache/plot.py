#!/usr/bin/env python3
"""One function per CSV in data/csv -> one PDF in plots/. Error bars = std over repetitions."""
from pathlib import Path
import numpy as np, pandas as pd, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).parent
PEAK = 6400e6 * 8 * 2 / 1e9            # DDR5-6400, 2 channels: 102.4 GB/s
L1, L2, L3 = 48, 1024, 32768           # KiB per core / per core / shared (lscpu)
try: GHZ = float((ROOT/"data/freq_ghz.txt").read_text())
except Exception: GHZ = float("nan")

def load(n): return pd.read_csv(ROOT/f"data/csv/{n}.csv", na_values=["<not supported>", "<not counted>"])
def save(fig, n): fig.tight_layout(); (ROOT/"plots").mkdir(exist_ok=True); fig.savefig(ROOT/f"plots/{n}.pdf"); plt.close(fig)

def gbar(ax, d, cat, val, ser=None, order=None):
    """Bars of mean(val) per cat (optionally split by ser) with std error bars."""
    if ser: g = d.pivot_table(index=cat, columns=ser, values=val, aggfunc=["mean", "std"]); m, s = g["mean"], g["std"]
    else:   g = d.groupby(cat)[val]; m, s = g.mean(), g.std()
    if order is not None: m, s = m.loc[order], s.loc[order]
    m.plot.bar(ax=ax, yerr=s, capsize=2, rot=20)

def levels(ax):
    for x, t in ((L1, "L1"), (L2, "L2"), (L3, "L3")): ax.axvline(x, c="gray", ls=":", lw=.8); ax.text(x, .98, t, transform=ax.get_xaxis_transform(), fontsize=7, va="top")

def zero_queue():
    d = load("zero_queue"); g = d.groupby("level").ns.agg(["mean", "std"]).loc[["L1", "L2", "L3", "DRAM"]]
    fig, ax = plt.subplots(figsize=(5, 3.5))
    ax.bar(g.index, g["mean"], yerr=g["std"], capsize=3); ax.set_yscale("log"); ax.set_ylim(top=g["mean"].max()*4)
    for i, m in enumerate(g["mean"]): ax.text(i, m*1.15, f"{m:.1f} ns" + (f"\n{m*GHZ:.0f} cycles" if GHZ == GHZ else ""), ha="center", fontsize=8)
    ax.set_ylabel("zero-queue latency (ns, log)"); ax.set_title(f"Zero-queue latency (core clock {GHZ:.2f} GHz observed)")
    save(fig, "zero_queue")

def pattern_gran():
    d = load("pattern_gran"); d["label"] = np.where(d.pattern == "rand", "random", "seq " + d.stride_b.astype(str) + "B"); d["bw"] = d.bw_mbps/1e3
    o = d.label.unique(); fig, ax = plt.subplots(1, 2, figsize=(10, 3.8))
    gbar(ax[0], d, "label", ["idle_ns", "loaded_ns"], order=o); ax[0].set_ylabel("latency (ns)"); ax[0].set_title("Latency: idle chase vs under max load")
    gbar(ax[1], d, "label", "bw", order=o); ax[1].set_ylabel("read bandwidth (GB/s)"); ax[1].set_title("Loaded read bandwidth (delay 0, 7 threads)")
    save(fig, "pattern_gran")

def rw_mix():
    d = load("rw_mix"); d["bw"] = d.bw_mbps/1e3; o = d.mix.unique(); fig, ax = plt.subplots(1, 2, figsize=(10, 3.8))
    gbar(ax[0], d, "mix", "bw", order=o); ax[0].axhline(PEAK, c="gray", ls="--"); ax[0].set_ylabel("bandwidth (GB/s)"); ax[0].set_title("Throughput (dashed: 102.4 GB/s DRAM peak)")
    gbar(ax[1], d, "mix", "lat_ns", order=o); ax[1].set_ylabel("loaded latency (ns)"); ax[1].set_title("Latency at max injection")
    save(fig, "rw_mix")

def intensity():
    d = load("intensity"); g = d.groupby("delay")[["lat_ns", "bw_mbps"]].agg(["mean", "std"]).sort_index(ascending=False)
    bw, lat = g[("bw_mbps", "mean")]/1e3, g[("lat_ns", "mean")]
    knee = bw[lat <= 1.5*lat.min()].max()                       # last point before latency exceeds 1.5x idle
    fig, ax = plt.subplots(figsize=(6.5, 4.2))
    ax.errorbar(bw, lat, xerr=g[("bw_mbps", "std")]/1e3, yerr=g[("lat_ns", "std")], fmt="o-", ms=3, capsize=2, label="loaded latency")
    ax.axvline(knee, c="r", ls=":", label=f"knee (lat = 1.5x idle): {knee:.0f} GB/s")
    ax.axvline(PEAK, c="gray", ls="--", label="DRAM theoretical peak")
    ax.set_xlabel("throughput (GB/s)"); ax.set_ylabel("latency (ns)")
    ax2 = ax.twinx(); ax2.plot(bw, bw*lat/64, "s--", c="g", ms=3, label="Little: BW x lat / 64 B"); ax2.set_ylabel("outstanding lines (Little's Law)", color="g")
    h = ax.get_legend_handles_labels(); h2 = ax2.get_legend_handles_labels(); ax.legend(h[0]+h2[0], h[1]+h2[1], fontsize=7, loc="upper left")
    ax.set_title("Throughput-latency (MLC loaded_latency, read-only)"); save(fig, "intensity")

def working_set():
    d = load("working_set"); g = d.groupby(["metric", "size_kb"]).value.agg(["mean", "std"])
    fig, ax = plt.subplots(figsize=(7, 4)); a = g.loc["lat_ns"]; b = g.loc["bw_gbps"]
    ax.errorbar(a.index, a["mean"], yerr=a["std"], fmt="o-", ms=3, capsize=2, label="latency (MLC, random chase)"); ax.set_yscale("log")
    ax.set_xscale("log"); ax.set_xlabel("working set (KiB; kern = total of 3 arrays)"); ax.set_ylabel("latency (ns)")
    ax2 = ax.twinx(); ax2.errorbar(b.index, b["mean"], yerr=b["std"], fmt="s--", c="C1", ms=3, capsize=2, label="1-thread seq BW (kern)"); ax2.set_ylabel("bandwidth (GB/s)", c="C1")
    levels(ax); h = ax.get_legend_handles_labels(); h2 = ax2.get_legend_handles_labels(); ax.legend(h[0]+h2[0], h[1]+h2[1], fontsize=7, loc="center left")
    ax.set_title("Working-set sweep"); save(fig, "working_set")

def cache_miss():
    d = load("cache_miss"); d["fp"] = d.total_bytes/1024
    for c in ("l1_miss", "llc_miss"): d[c+"_pe"] = d[c]/d.elems
    fig, ax = plt.subplots(1, 3, figsize=(15, 4))
    for p, g in d.groupby("pattern"):
        m = g.groupby("fp")[["ns_per_elem", "l1_miss_pe", "llc_miss_pe"]].agg(["mean", "std"])
        ax[0].errorbar(m.index, m[("ns_per_elem", "mean")], yerr=m[("ns_per_elem", "std")], marker="o", ms=3, capsize=2, label=p)
        ax[1].plot(m.index, m[("l1_miss_pe", "mean")], "o-", ms=3, label=f"{p}: L1D load miss")
        ax[1].plot(m.index, m[("llc_miss_pe", "mean")], "s--", ms=3, label=f"{p}: cache-misses")
        ax[2].scatter(g.llc_miss_pe, g.ns_per_elem, s=8, label=p)
    for a in ax[:2]: a.set_xscale("log"); a.set_xlabel("footprint (KiB, 3 arrays)"); levels(a)
    ax[0].set_yscale("log"); ax[0].set_ylabel("ns / element"); ax[1].set_ylabel("miss events / element (incl. init)"); ax[1].set_yscale("log")
    ax[2].set_xlabel("cache-misses / element"); ax[2].set_ylabel("ns / element"); ax[2].set_xscale("log"); ax[2].set_yscale("log")
    for a in ax: a.legend(fontsize=7)
    fig.suptitle("Cache-miss impact (z=x*y, 4 KiB pages, 1 thread)"); save(fig, "cache_miss")

def tlb():
    d = load("tlb"); d["fp"] = d.array_bytes/2**20; d["tlb_pe"] = d.tlb_miss/d.elems
    d["lab"] = np.where(d.pattern == "stride", "stride " + d.stride_b.astype(str) + "B", d.pattern)
    a, b = d[d["sweep"] == "pat"], d[d["sweep"] == "fp"]; o = a.lab.unique()
    fig, ax = plt.subplots(2, 2, figsize=(11, 7))
    gbar(ax[0, 0], a, "lab", "ns_per_elem", "pages", o); ax[0, 0].set_ylabel("ns / element"); ax[0, 0].set_title("256 MiB arrays: time by access pattern")
    gbar(ax[0, 1], a, "lab", "tlb_pe", "pages", o); ax[0, 1].set_ylabel("dTLB misses / element"); ax[0, 1].set_title("256 MiB arrays: dTLB misses")
    for pg, g in b.groupby("pages"):
        m = g.groupby("fp")[["ns_per_elem", "tlb_pe"]].agg(["mean", "std"])
        ax[1, 0].errorbar(m.index, m[("ns_per_elem", "mean")], yerr=m[("ns_per_elem", "std")], marker="o", ms=3, capsize=2, label=f"{pg} pages")
        ax[1, 1].errorbar(m.index, m[("tlb_pe", "mean")], yerr=m[("tlb_pe", "std")], marker="o", ms=3, capsize=2, label=f"{pg} pages")
    for a_, y in ((ax[1, 0], "ns / element"), (ax[1, 1], "dTLB misses / element")):
        a_.set_xscale("log"); a_.set_xlabel("array size (MiB), 4 KiB stride"); a_.set_ylabel(y); a_.legend(fontsize=7)
    fig.suptitle("TLB impact: 4 KiB vs 2 MiB pages"); save(fig, "tlb")

def pitfall():
    d = load("pitfall"); a, b = d[d.pitfall == "cache_reuse"], d[d.pitfall == "pin_governor"]
    fig, ax = plt.subplots(1, 2, figsize=(10, 3.8))
    gbar(ax[0], a, "variant", "gbps", order=a.variant.unique()); ax[0].axhline(PEAK, c="gray", ls="--"); ax[0].set_yscale("log")
    ax[0].set_ylabel("'read bandwidth' (GB/s, log)"); ax[0].set_title("Pitfall A: buffer size decides what MLC measures (dashed: DRAM peak)")
    o = b.variant.unique(); gbar(ax[1], b, "variant", "ns_per_elem", order=o); ax[1].set_ylabel("ns / element")
    for i, v in enumerate(o): ax[1].text(i, ax[1].get_ylim()[1]*.9, f"{b[b.variant == v].ghz.mean():.2f} GHz", ha="center", fontsize=8)
    ax[1].set_title("Pitfall B: pinning / governor (cache-resident kernel)"); save(fig, "pitfall")

if __name__ == "__main__":
    for f in (zero_queue, pattern_gran, rw_mix, intensity, working_set, cache_miss, tlb, pitfall):
        try: f(); print("ok  ", f.__name__)
        except FileNotFoundError: print("skip", f.__name__, "(no csv)")
