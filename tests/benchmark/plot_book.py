import csv
import collections
import math
import matplotlib.pyplot as plt

infile = "results.csv"
out_png = "results.png"

data = collections.defaultdict(list)

with open(infile, newline="") as f:
    r = csv.DictReader(f)
    for row in r:
        op = row["op"]
        size = int(row["size"])
        val = float(row["ns_per_op"])
        data[op].append((size, val))

plt.figure(figsize=(8, 5))
for op, lst in data.items():
    # Remove top 10% highest values (outliers) per operation
    n = len(lst)
    if n == 0:
        continue
    # remove_count is the number of highest samples to drop
    remove_count = int(math.ceil(n * 0.10))
    if remove_count >= n:
        trimmed = lst
    else:
        # sort by value (ns_per_op), drop the highest `remove_count`
        trimmed = sorted(lst, key=lambda x: x[1])[: n - remove_count]
    # sort by size for plotting
    trimmed.sort(key=lambda x: x[0])
    # Subsample every 10th point after filtering to reduce plot density
    sampled = trimmed[::10]
    if not sampled and trimmed:
        sampled = [trimmed[-1]]
    xs = [s for s, _ in sampled]
    ys = [v for _, v in sampled]
    plt.plot(xs, ys, marker="o", label=op)

# plt.xscale('log')
# plt.yscale('log')
plt.xlabel("book size")
plt.ylabel("avg time per op (ns)")
plt.title("LOB benchmarks")
plt.legend()
plt.grid(True, which="both", ls="--", lw=0.5)
plt.tight_layout()
plt.savefig(out_png)
print("Wrote", out_png)
