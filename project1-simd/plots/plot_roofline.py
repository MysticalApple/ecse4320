#!/usr/bin/env python3

import sys
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


# Read CSV without a header row.
df = pd.read_csv(sys.argv[1], header=None, names=COLUMNS)

# Convert relevant fields to numeric values.
numeric_columns = [
    "n", "align_offset", "stride", "rep",
    "gflops", "gbps", "checksum_ok",
]
for col in numeric_columns:
    df[col] = pd.to_numeric(df[col], errors="coerce")

# Apply benchmark validity filters.
df = df[
    (df["align_offset"] == 0)
    & (df["stride"] == 1)
    & (df["checksum_ok"] == 1)
].copy()

# Remove invalid values; log axes require positive intensity and GFLOPS.
df = df[
    (df["gflops"] > 0)
    & (df["gbps"] > 0)
    & np.isfinite(df["gflops"])
    & np.isfinite(df["gbps"])
].copy()

if df.empty:
    raise SystemExit("No valid rows remain after filtering.")

# GFLOPS / GB/s = FLOPs / byte.
df["arithmetic_intensity"] = df["gflops"] / df["gbps"]

# Aggregate repetitions at each kernel/variant/dtype/problem size.
grouped = (
    df.groupby(
        ["kernel", "variant", "dtype", "n"],
        as_index=False,
    )[["gflops", "gbps", "arithmetic_intensity"]]
    .median()
)

# Give each kernel/variant combination a distinct style.
configs = sorted(
    set(zip(grouped["kernel"], grouped["variant"]))
)
colors = plt.get_cmap("turbo", max(len(configs), 1))
markers = ["o", "s", "^", "D", "v", "P", "X", "<", ">", "*"]

fig, ax = plt.subplots(figsize=(11, 7))

for i, (kernel, variant) in enumerate(configs):
    subset = grouped[
        (grouped["kernel"] == kernel)
        & (grouped["variant"] == variant)
    ]

    color = colors(i)
    marker = markers[i % len(markers)]

    ax.scatter(
        subset["arithmetic_intensity"],
        subset["gflops"],
        color=[color],
        marker=marker,
        s=65,
        alpha=0.85,
        label=f"{kernel} / {variant}",
        edgecolors="black",
        linewidths=0.4,
    )

ax.set_xscale("linear")
ax.set_yscale("linear")
ax.set_xlabel("Arithmetic intensity (FLOPs / byte)")
ax.set_ylabel("Performance (GFLOPS)")
ax.set_title("Roofline-Style Performance Diagram")
ax.grid(True, which="both", linestyle=":", alpha=0.4)

ax.legend(
    title="Kernel / variant",
    fontsize="small",
    title_fontsize="small",
    bbox_to_anchor=(1.02, 1),
    loc="upper left",
)


fig.tight_layout()
plt.show()

fig.savefig("roofline.png", dpi=200, bbox_inches="tight")


plt.close(fig)

print(f"saved ./roofline.png")
