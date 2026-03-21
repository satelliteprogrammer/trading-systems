# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "matplotlib>=3",
# ]
# ///
"""
Plot LOB benchmark results from results.csv.

Renders average nanoseconds per operation against book size for each
operation type, dropping the top 20% of values per series as outliers.
"""

import collections
import csv

import matplotlib.pyplot as plt


infile = "results.csv"
out_png = "results.png"

data: dict[str, list[int]] = collections.defaultdict(list)

with open(infile, newline="") as f:
    r = csv.DictReader(f)
    for row in r:
        data[row["op"]].append(int(row["ns_per_op"]))

plt.figure(figsize=(10, 5))

print("Benchmark results:")


plt.figure(figsize=(8, 5))
ax = plt.gca()
for op, lst in data.items():
    # Remove top 20% highest values (outliers) per benchmark series
    n_drop = int(len(lst) * 0.2)
    if n_drop > 0:
        # drop entries with the largest 'val' values
        by_val = sorted(lst, key=lambda x: x[1])
        kept = by_val[:-n_drop]
    else:
        kept = list(lst)

    if not kept:
        continue

    # sort remaining points by size for plotting
    kept.sort(key=lambda x: x[0])
    xs = [s for s, _ in kept]
    ys = [v for _, v in kept]
    # Draw line segments only between adjacent (sorted) points, then draw markers on top
    seg_color = None
    if len(xs) >= 2:
        # draw first segment to acquire a color from the cycle
        (ln,) = ax.plot(
            [xs[0], xs[1]],
            [ys[0], ys[1]],
            linestyle="-",
            linewidth=1,
            label=op,
            zorder=1,
        )
        seg_color = ln.get_color()
        for i in range(1, len(xs) - 1):
            ax.plot(
                [xs[i], xs[i + 1]],
                [ys[i], ys[i + 1]],
                linestyle="-",
                linewidth=1,
                color=seg_color,
                zorder=1,
            )

    # Draw markers on top using the same color as the segments when available
    ax.scatter(xs, ys, marker="o", s=36, color=seg_color, zorder=2)

plt.xlabel("book size")
plt.ylabel("avg time per op (ns)")
plt.title("LOB benchmarks")
plt.legend()
plt.grid(True, which="both", ls="--", lw=0.5)
plt.tight_layout()
plt.savefig(out_png)
print("Wrote", out_png)
