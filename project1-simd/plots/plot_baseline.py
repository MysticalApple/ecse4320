#!/usr/bin/env python3

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


COLUMNS = [
    "kernel",
    "variant",
    "dtype",
    "n",
    "align_offset",
    "stride",
    "rep",
    "time_ns",
    "gflops",
    "gbps",
    "checksum_ok",
]



# Read CSV. The input is expected to have no header row.
df = pd.read_csv("data.csv", header=None, names=COLUMNS)

# Ensure numeric columns are parsed correctly.
numeric_columns = [
    "n", "align_offset", "stride", "rep",
    "time_ns", "gflops", "gbps", "checksum_ok",
]
for col in numeric_columns:
    df[col] = pd.to_numeric(df[col], errors="coerce")

# Apply requested filters.
df = df[
    (df["align_offset"] == 0)
    & (df["stride"] == 1)
    & (df["checksum_ok"] == 1)
].copy()

# Log-log axes require positive x and y values.
df = df[
    (df["n"] > 0)
    & (df["time_ns"] > 0)
    & np.isfinite(df["n"])
    & np.isfinite(df["time_ns"])
]

if df.empty:
    raise SystemExit("No valid rows remain after filtering.")

# Use a consistent color for each variant across all kernels.
variants = sorted(df["variant"].unique())
cmap = plt.get_cmap("tab10", max(len(variants), 1))
variant_colors = {
    variant: cmap(i) for i, variant in enumerate(variants)
}

for kernel, kernel_df in df.groupby("kernel"):
    fig, ax = plt.subplots(figsize=(9, 6))

    for variant, variant_df in kernel_df.groupby("variant"):
        color = variant_colors[variant]

        # Median and interquartile range of repeated measurements.
        grouped = (
            variant_df.groupby("n")["time_ns"]
            .agg(
                median="median",
                q25=lambda s: s.quantile(0.25),
                q75=lambda s: s.quantile(0.75),
            )
            .reset_index()
            .sort_values("n")
        )

        x = grouped["n"].to_numpy(dtype=float)
        y = grouped["median"].to_numpy(dtype=float)

        # Asymmetric error bars: distance from median to each quartile.
        yerr = np.vstack([
            y - grouped["q25"].to_numpy(),
            grouped["q75"].to_numpy() - y,
        ])

        ax.errorbar(
            x, y,
            yerr=yerr,
            color=color,
            label=f"{variant}",
            alpha=0.85,
            marker="o",
            linestyle="-",
            capsize=3,
            markersize=5,
        )

    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xlabel("Working set size")
    ax.set_ylabel("Time (ns)")
    ax.set_title(f"Kernel: {kernel}")
    ax.grid(True, which="both", linestyle=":", alpha=0.4)
    ax.legend(fontsize="small")
    fig.tight_layout()

    # Sanitize the kernel name for use as a filename.
    safe_kernel = "".join(
        c if c.isalnum() or c in "-_." else "_"
        for c in str(kernel)
    )
    output_file = f"./plots/baseline_{safe_kernel}.png"

    fig.savefig(output_file, dpi=200)
    plt.close(fig)

    print(f"saved {output_file}")
