#!/usr/bin/env python3

import argparse
from pathlib import Path
import sys

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator


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
df = pd.read_csv(sys.argv[1], header=None, names=COLUMNS)

# Ensure numeric columns are parsed correctly.
numeric_columns = [
    "n", "align_offset", "stride", "rep",
    "time_ns", "gflops", "gbps", "checksum_ok",
]
for col in numeric_columns:
    df[col] = pd.to_numeric(df[col], errors="coerce")

# Apply requested filters.
df = df[
    (df["checksum_ok"] == 1)
    & (df["rep"] != 0)
].copy()

if df.empty:
    raise SystemExit("No valid rows remain after filtering.")

# Use a consistent color for each variant across all kernels.
variants = sorted(df["variant"].unique())
cmap = plt.get_cmap("tab10", max(len(variants), 1))
variant_colors = {
    variant: cmap(i) for i, variant in enumerate(variants)
}

legend = False
fig, ax = plt.subplots(figsize=(9, 6))
for kernel, kernel_df in df.groupby("kernel"):
    # fig, ax = plt.subplots(figsize=(9, 6))

    for variant, variant_df in kernel_df.groupby("variant"):
        color = variant_colors[variant]

        # Median and interquartile range of repeated measurements.
        grouped = (
            variant_df.groupby("dtype")["gflops"]
            .agg(
                median="median",
                q25=lambda s: s.quantile(0.25),
                q75=lambda s: s.quantile(0.75),
            )
            .reset_index()
            .sort_values("dtype")
        )

        x = [kernel + "_" + d for d in grouped["dtype"]]
        y = grouped["median"].to_numpy(dtype="float")


        # Asymmetric error bars: distance from median to each quartile.
        yerr = np.vstack([
            y - grouped["q25"].to_numpy(),
            grouped["q75"].to_numpy() - y,
        ])

        ax.bar(
            x, y,
            yerr=yerr,
            color=color,
            label=f"{variant}",
            alpha=0.85,
            linestyle="",
            capsize=3,
        )


        # ax.errorbar(
        #     x, y,
        #     yerr=yerr,
        #     color=color,
        #     label=f"{variant}",
        #     marker="+",
        #     linestyle="",
        #     capsize=3,
        #     markersize=5,
        #     alpha=0.85,
        # )

    # Only make legend once
    if not legend:
        ax.legend(fontsize="small")
        legend = True

    # ax.set_xscale("linear")
    ax.set_yscale("linear")
    ax.set_xlabel("Kernel and Datatype")
    ax.set_ylabel("GFLOPS")
    ax.set_title(f"Single- vs. Double-precision Floating Point Performance")
    ax.grid(True, which="both", linestyle=":", alpha=0.4)
    fig.tight_layout()


# plt.show()

output_file = f"./datatype.png"

fig.savefig(output_file, dpi=200)
plt.close(fig)

print(f"saved {output_file}")

