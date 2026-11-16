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

for kernel, kernel_df in df.groupby("kernel"):
    fig, ax = plt.subplots(figsize=(9, 6))

    variants = sorted(kernel_df["variant"].unique())
    n_variants = len(variants)

    # Positions for each stride group.
    strides = sorted(kernel_df["stride"].unique(), key=lambda s: (s == 0, s))
    x = np.arange(len(strides))

    # Width of each individual bar.
    width = 0.8 / n_variants

    # Labels for the x-axis.
    labels = [str(s) for s in strides]
    labels[-1] = "Gather"

    for i, variant in enumerate(variants):
        variant_df = kernel_df[kernel_df["variant"] == variant]
        color = variant_colors[variant]

        grouped = (
            variant_df.groupby("stride")["gbps"]
            .agg(
                median="median",
                q25=lambda s: s.quantile(0.25),
                q75=lambda s: s.quantile(0.75),
            )
            .reindex(strides)
        )

        y = grouped["median"].to_numpy(dtype=float)

        yerr = np.vstack([
            y - grouped["q25"].to_numpy(),
            grouped["q75"].to_numpy() - y,
        ])

        # Offset each variant within its stride group.
        offset = (i - (n_variants - 1) / 2) * width

        ax.bar(
            x + offset,
            y,
            width=width,
            yerr=yerr,
            color=color,
            label=variant,
            alpha=0.85,
            capsize=3,
        )

    ax.set_xticks(x)
    ax.set_xticklabels(labels)

    ax.set_yscale("linear")
    ax.set_xlabel("Stride (n)")
    ax.set_ylabel("GBPS")
    ax.set_title(f"Kernel: {kernel}")
    ax.grid(True, which="both", linestyle=":", alpha=0.4)
    ax.set_axisbelow(True)
    ax.legend(fontsize="small")

    fig.tight_layout()

    safe_kernel = "".join(
        c if c.isalnum() or c in "-_." else "_"
        for c in str(kernel)
    )
    output_file = f"./stride_{safe_kernel}.pdf"

    fig.savefig(output_file, dpi=200)
    plt.close(fig)

    print(f"saved {output_file}")


