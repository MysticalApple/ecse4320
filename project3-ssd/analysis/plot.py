#!/usr/bin/env python3
"""results/processed/summary.csv -> PDFs in results/plots. Usage: plot.py [results_dir]
Error bars are the standard deviation over repetitions. Plots whose data is missing are skipped."""
import csv, glob, os, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RES = sys.argv[1] if len(sys.argv) > 1 else "results"
OUT = f"{RES}/plots"
os.makedirs(OUT, exist_ok=True)
plt.rcParams.update({"axes.grid": True, "grid.alpha": 0.3, "legend.fontsize": 8})


def load(path):
    rows = list(csv.DictReader(open(path)))
    for r in rows:
        for k, v in r.items():
            try:
                r[k] = float(v)
            except ValueError:
                pass
    return rows


ROWS = load(f"{RES}/processed/summary.csv")


def sel(exp, **kw):
    return [r for r in ROWS if r["exp"] == exp and all(r[k] == v for k, v in kw.items())]


def mv(rs, k):  # (means, stds) of metric k
    return np.array([r[k + "_mean"] for r in rs]), np.array([r[k + "_std"] for r in rs])


def col(rs, k):
    return np.array([r[k] for r in rs])


def save(fig, name):
    fig.tight_layout()
    fig.savefig(f"{OUT}/{name}")
    plt.close(fig)
    print("wrote", f"{OUT}/{name}")


def bars(ax, labels, series):  # series: {name: (means, stds)}
    w = 0.8 / len(series)
    for i, (n, (m, e)) in enumerate(series.items()):
        ax.bar(np.arange(len(labels)) + i * w, m, w, yerr=e, capsize=2, label=n)
    ax.set_xticks(np.arange(len(labels)) + 0.4 - w / 2)
    ax.set_xticklabels(labels)


WL = [("randread", "4k", "4K rand read"), ("randwrite", "4k", "4K rand write"),
      ("read", "128k", "128K seq read"), ("write", "128k", "128K seq write")]


def baseline():
    rs = [(n, sel("baseline", rw=rw, bs=bs, qd=1)) for rw, bs, n in WL]
    rs = [(n, r[0]) for n, r in rs if r]
    if not rs:
        return
    stats = [("mean", "lat_mean_us"), ("p50", "p50"), ("p95", "p95"), ("p99", "p99"), ("p99.9", "p99.9")]
    fig, ax = plt.subplots(figsize=(8, 4))
    bars(ax, [n for n, _ in rs],
         {lab: ([r[k + "_mean"] for _, r in rs], [r[k + "_std"] for _, r in rs]) for lab, k in stats})
    ax.set_yscale("log"); ax.set_ylabel("latency (µs)"); ax.legend()
    ax.set_title("Zero-queue baseline (QD=1, direct I/O)")
    save(fig, "baseline_latency.pdf")
    with open(f"{RES}/processed/baseline_table.csv", "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["workload", "IOPS", "MB/s"] + [f"{lab} (us)" for lab, _ in stats])
        for n, r in rs:
            w.writerow([n, round(r["iops_mean"]), round(r["bw_MBs_mean"], 1)] +
                       [round(r[k + "_mean"], 1) for _, k in stats])


def blocksize():
    ticks = [4, 16, 64, 256, 1024]
    for qd in (1, 32):
        fig, axs = plt.subplots(2, 2, figsize=(9, 6), sharex=True)
        for j, (pat, rws) in enumerate((("random", ("randread", "randwrite")), ("sequential", ("read", "write")))):
            axs[0, j].set_title(pat)
            for rw in rws:
                rs = sorted(sel("blocksize", rw=rw, qd=qd), key=lambda r: r["bs_KiB"])
                if not rs:
                    continue
                x = col(rs, "bs_KiB")
                axs[0, j].errorbar(x, *mv(rs, "bw_MBs"), marker="o", capsize=2, label=rw)
                ln = axs[1, j].errorbar(x, *mv(rs, "lat_mean_us"), marker="o", capsize=2, label=f"{rw} mean")
                axs[1, j].errorbar(x, *mv(rs, "p99"), ls="--", marker="s", capsize=2,
                                   color=ln[0].get_color(), label=f"{rw} p99")
            axs[0, j].set_ylabel("throughput (MB/s)"); axs[1, j].set_ylabel("latency (µs)")
            axs[1, j].set_yscale("log"); axs[1, j].set_xlabel("block size")
            axs[0, j].legend(); axs[1, j].legend()
            axs[1, j].set_xscale("log", base=2); axs[1, j].minorticks_off()
            axs[1, j].set_xticks(ticks); axs[1, j].set_xticklabels(["4K", "16K", "64K", "256K", "1M"])
        fig.suptitle(f"Block-size sweep, effective QD={qd}")
        save(fig, f"blocksize_qd{qd}.pdf")


def rwmix():
    rs = sorted(sel("rwmix"), key=lambda r: r["mix"])
    if not rs:
        return
    fig, axs = plt.subplots(1, 3, figsize=(13, 4))
    for ax, (k, lab) in zip(axs, [("iops", "IOPS"), ("lat_mean_us", "mean latency (µs)"), ("p99", "p99 latency (µs)")]):
        for d, name in (("r_", "read"), ("w_", "write")):
            keep = [r for r in rs if r[d + "iops_mean"] > 0]
            if keep:
                ax.errorbar(col(keep, "mix"), *mv(keep, d + k), marker="o", capsize=2, label=name)
        if k == "iops":
            ax.errorbar(col(rs, "mix"), *mv(rs, "iops"), ls=":", marker="o", capsize=2, label="total")
        ax.set_xlabel("read share of the mix (%)"); ax.set_ylabel(lab); ax.legend()
        if k != "iops":
            ax.set_yscale("log")
    fig.suptitle("Read/write mix, 4K random, QD=32")
    save(fig, "rwmix.pdf")


QDW = [("randread", "4k", "4K random read", "iops", 1e3, "kIOPS"),
       ("randwrite", "4k", "4K random write", "iops", 1e3, "kIOPS"),
       ("read", "128k", "128K sequential read", "bw_MBs", 1, "MB/s")]


def qd_rows(rw, bs):
    return sorted(sel("qdsweep", rw=rw, bs=bs), key=lambda r: r["qd"])


def qd_curve():
    fig, axs = plt.subplots(1, 3, figsize=(13, 4))
    for ax, (rw, bs, name, k, sc, unit) in zip(axs, QDW):
        rs = qd_rows(rw, bs)
        ax.set_title(name)
        if not rs:
            continue
        thr, te = (a / sc for a in mv(rs, k))
        lat, le = mv(rs, "lat_mean_us")
        p99, _ = mv(rs, "p99")
        ax.errorbar(lat, thr, xerr=le, yerr=te, marker="o", capsize=2, label="mean latency")
        ax.plot(p99, thr, "--s", alpha=0.7, label="p99 latency")
        for r, x, y in zip(rs, lat, thr):
            ax.annotate(f"{int(r['qd'])}", (x, y), textcoords="offset points", xytext=(4, -10), fontsize=7)
        i = next(i for i, t in enumerate(thr) if t >= 0.9 * thr.max())  # knee: first QD with >=90% of peak
        ax.plot(lat[i], thr[i], "*", ms=16, color="red", label=f"knee: QD={int(rs[i]['qd'])} (>=90% of peak)")
        ax.set_xscale("log"); ax.set_xlabel("latency (µs); labels = effective QD"); ax.set_ylabel(unit)
        ax.legend(loc="lower right")
    fig.suptitle("Throughput vs latency as queue depth grows")
    save(fig, "qd_curve.pdf")


def littles():
    fig, axs = plt.subplots(1, 3, figsize=(13, 4))
    qds = None
    for rw, bs, name, *_ in QDW:
        rs = qd_rows(rw, bs)
        if not rs:
            continue
        qds = col(rs, "qd")
        iops, _ = mv(rs, "iops")
        lat, _ = mv(rs, "lat_mean_us")
        ln, = axs[0].plot(qds, iops * lat / 1e6, "o-", label=name)
        c = ln.get_color()
        axs[1].plot(qds, iops, "o-", color=c, label=name)
        axs[1].plot(qds, qds * iops[0] / qds[0], ":", color=c)  # ideal scaling from the QD=1 point
        axs[2].plot(qds, col(rs, "usr_cpu_mean") + col(rs, "sys_cpu_mean"), "o-", color=c, label=name)
    if qds is None:
        plt.close(fig)
        return
    axs[0].plot(qds, qds, "k--", label="outstanding = QD")
    axs[0].set_ylabel("IOPS x mean latency (outstanding IOs)")
    axs[1].set_ylabel("IOPS"); axs[1].set_yscale("log")
    axs[1].set_title("dotted: ideal linear scaling from QD=1", fontsize=9)
    axs[2].set_ylabel("fio usr+sys CPU (%)")
    axs[0].set_title("Little's Law check", fontsize=9)
    axs[2].set_title("CPU-bound check", fontsize=9)
    for ax in axs:
        ax.set_xscale("log", base=2); ax.set_xlabel("effective queue depth"); ax.legend()
    axs[0].set_yscale("log")
    save(fig, "qd_littles_law.pdf")


def tail():
    P = [50, 75, 90, 95, 99, 99.5, 99.9, 99.95, 99.99, 99.999]
    x = [-np.log10(1 - p / 100) for p in P]
    fig, axs = plt.subplots(1, 2, figsize=(11, 4))
    for ax, rw in zip(axs, ("randread", "randwrite")):
        for r in sorted(sel("tail", rw=rw), key=lambda r: r["qd"]):
            ax.errorbar(x, [r[f"p{p:g}_mean"] for p in P], [r[f"p{p:g}_std"] for p in P],
                        marker="o", capsize=2, label=f"QD={int(r['qd'])}")
        ax.set_xticks([x[i] for i in (0, 2, 4, 6, 8, 9)])
        ax.set_xticklabels(["50", "90", "99", "99.9", "99.99", "99.999"])
        ax.set_yscale("log"); ax.set_xlabel("percentile"); ax.set_ylabel("latency (µs)")
        ax.set_title(f"4K {rw}"); ax.legend()
    fig.suptitle("Tail latency")
    save(fig, "tail.pdf")


def lba_range():
    if not sel("range"):
        return
    fig, axs = plt.subplots(1, 2, figsize=(10, 4))
    for rw in ("randread", "randwrite"):
        rs = sorted(sel("range", rw=rw), key=lambda r: r["range"])
        if not rs:
            continue
        x = col(rs, "range")
        axs[0].errorbar(x, *mv(rs, "iops"), marker="o", capsize=2, label=rw)
        ln = axs[1].errorbar(x, *mv(rs, "lat_mean_us"), marker="o", capsize=2, label=f"{rw} mean")
        axs[1].errorbar(x, *mv(rs, "p99"), ls="--", marker="s", capsize=2, color=ln[0].get_color(), label=f"{rw} p99")
    for ax, lab in zip(axs, ("IOPS", "latency (µs)")):
        ax.set_xscale("log", base=2); ax.minorticks_off(); ax.set_xticks([1, 8, 64, 256])
        ax.set_xticklabels(["1", "8", "64", "256"]); ax.set_xlabel("LBA range touched (GiB)")
        ax.set_ylabel(lab); ax.legend()
    axs[1].set_yscale("log")
    fig.suptitle("Working-set / LBA-range sweep, 4K random, QD=32")
    save(fig, "range.pdf")


def pitfall():
    tags = [("direct", "direct=1\nio_uring, QD 32"), ("buffered", "direct=0\nbuffered, QD 32"),
            ("psync", "direct=1\npsync (effective QD 1)")]
    rs = [(lab, sel("pitfall", tag=t)) for t, lab in tags]
    rs = [(lab, r[0]) for lab, r in rs if r]
    if not rs:
        return
    fig, axs = plt.subplots(1, 2, figsize=(10, 4))
    for ax, (k, lab) in zip(axs, (("iops", "IOPS"), ("lat_mean_us", "mean latency (µs)"))):
        bars(ax, [n for n, _ in rs], {"": ([r[k + "_mean"] for _, r in rs], [r[k + "_std"] for _, r in rs])})
        for b in ax.patches:
            ax.annotate((f"{b.get_height():,.0f}" if b.get_height() >= 100 else f"{b.get_height():.1f}"), (b.get_x() + b.get_width() / 2, b.get_height()),
                        ha="center", va="bottom", fontsize=8)
        ax.set_yscale("log"); ax.set_ylabel(lab); ax.tick_params(axis="x", labelsize=8)
    fig.suptitle("Same drive, same 4K random read over 4 GiB: three setups")
    save(fig, "pitfall.pdf")


def state_timeseries():
    d = f"{RES}/raw/state"
    bw = glob.glob(f"{d}/steady_bw*.log")
    if not bw:
        return
    a = np.loadtxt(bw[0], delimiter=",", usecols=(0, 1), ndmin=2)
    fig, ax = plt.subplots(figsize=(9, 4))
    ax.plot(a[:, 0] / 1000, a[:, 1] * 1024 / 1e6, label="write bandwidth")
    ax.set_xlabel("time (s)"); ax.set_ylabel("sequential write bandwidth (MB/s)"); ax.set_ylim(bottom=0)
    tf = f"{d}/steady_temp.log"
    if os.path.exists(tf):
        t = np.genfromtxt(tf, ndmin=2)
        if t.shape[1] == 2:
            ax2 = ax.twinx()
            ax2.plot(t[:, 0], t[:, 1], color="tab:red", alpha=0.7)
            ax2.set_ylabel("NVMe temperature (°C)", color="tab:red"); ax2.grid(False)
    ax.set_title("Sustained 1M sequential write (QD 32)")
    save(fig, "state_timeseries.pdf")


def state_compare():
    labels, series = [], {"before": ([], []), "after": ([], [])}
    for rw in ("randread", "randwrite"):
        b, a = sel("state", rw=rw, tag="before"), sel("state", rw=rw, tag="after")
        if not (b and a):
            continue
        labels.append(f"4K {rw}")
        for tag, r in (("before", b[0]), ("after", a[0])):
            for lst, k in zip(series[tag], ("iops", "p99")):
                lst.append((r[k + "_mean"], r[k + "_std"]))
    if not labels:
        return
    fig, axs = plt.subplots(1, 2, figsize=(10, 4))
    for ax, i, lab in zip(axs, (0, 1), ("IOPS", "p99 latency (µs)")):
        bars(ax, labels, {t: ([v[0] for v in s[i]], [v[1] for v in s[i]]) for t, s in series.items()})
        ax.set_ylabel(lab); ax.legend()
    fig.suptitle("Device state: before vs right after the sustained sequential write (QD=32)")
    save(fig, "state_compare.pdf")


for fn in (baseline, blocksize, rwmix, qd_curve, littles, tail, lba_range, pitfall, state_timeseries, state_compare):
    fn()
