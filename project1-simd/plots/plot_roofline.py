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
    & (df["rep"] != 0)
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

# Assign a consistent color to each kernel and marker to each variant.
kernels = sorted(grouped["kernel"].unique())
variants = sorted(grouped["variant"].unique())
dtypes = sorted(grouped["dtype"].unique())

cmap = plt.get_cmap("turbo", max(len(kernels), 1))
kernel_colors = {
    kernel: cmap(i)
    for i, kernel in enumerate(kernels)
}

markers = ["o", "s", "^", "D", "v", "P", "X", "<", ">", "*"]
variant_markers = {
    variant: markers[i % len(markers)]
    for i, variant in enumerate(variants)
}

fig, ax = plt.subplots(figsize=(11, 7))

for kernel in kernels:
    for variant in variants:
        for dtype in dtypes:
            subset = grouped[
                (grouped["kernel"] == kernel)
                & (grouped["variant"] == variant)
                & (grouped["dtype"] == dtype)
            ]

            if subset.empty:
                continue

            ax.scatter(
                subset["arithmetic_intensity"],
                subset["gflops"],
                color=kernel_colors[kernel],
                marker=variant_markers[variant],
                s=130 if dtype == "f64" else 65,
                alpha=0.85,
                label=f"{kernel} / {variant}" if dtype == "f64" else "",
                edgecolors="black",
                linewidths=0.4,
            )

max_bandwidth = 339
max_throughput = 89

# Determine the x-axis range.
x_min = grouped["arithmetic_intensity"].min()
x_max = grouped["arithmetic_intensity"].max()
x_line = np.linspace(x_min, x_max, 200)

# Maximum bandwidth: performance = bandwidth * arithmetic intensity.
ax.plot(
    x_line,
    max_bandwidth * x_line,
    linestyle="--",
    color="black",
    linewidth=1.5,
    label=f"Max bandwidth ({max_bandwidth} GB/s)",
)

# Maximum throughput: horizontal performance ceiling.
ax.axhline(
    y=max_throughput,
    linestyle="--",
    color="red",
    linewidth=1.5,
    label=f"Max throughput ({max_throughput} GFLOPS)",
)

ax.set_xscale("linear")
ax.set_yscale("linear")
ax.set_xlabel("Arithmetic intensity (FLOPs / byte)")
ax.set_ylabel("Performance (GFLOPS)")
ax.set_title("Roofline Performance Model")
ax.grid(True, which="both", linestyle=":", alpha=0.4)

ax.legend(
    title="Kernel / variant\nsmall=f32    big=f64",
    fontsize="medium",
    title_fontsize="medium",
    loc="upper left",
)


fig.tight_layout()

fig.savefig("roofline.pdf", dpi=200, bbox_inches="tight")


plt.close(fig)

print(f"saved ./roofline.pdf")
