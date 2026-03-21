# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "matplotlib>=3",
# ]
# ///
import collections
import csv
from pathlib import Path
import sys

import matplotlib.pyplot as plt

# Message type indices from std::variant<NewOrder, PartialCancel, Cancel, Execute, Halt>
MESSAGE_TYPES = {
    0: "NewOrder",
    1: "PartialCancel",
    2: "Cancel",
    3: "Execute",
    4: "Halt",
}


def read(fn: Path) -> dict[str, list[int]]:
    """Read CSV with format: message_type_index,nanoseconds (no header)"""
    ops = collections.defaultdict(list)

    with open(fn, newline="") as f:
        r = csv.reader(f)
        for row in r:
            if len(row) < 2:
                continue
            msg_type = int(row[0])
            ns = int(row[1])
            label = MESSAGE_TYPES.get(msg_type, f"type_{msg_type}")
            ops[label].append(ns)

    return ops


def main(fn: Path) -> None:
    data = read(fn)

    if not data:
        print("No data found in CSV")
        return

    # Filter to non-empty categories
    categories = [(k, v) for k, v in sorted(data.items()) if v]

    if not categories:
        print("No timing data found")
        return

    labels = [k for k, _ in categories]
    values = [v for _, v in categories]

    # Calculate overall range
    all_values = [ns for v in values for ns in v]
    min_val = min(all_values)
    max_val = max(all_values)

    # Cap at 99th percentile to avoid outliers dominating
    sorted_all = sorted(all_values)
    p99 = sorted_all[int(len(sorted_all) * 0.99)]
    max_val = min(max_val, p99 * 1.5)

    fig, ax = plt.subplots(figsize=(12, 6))

    ax.hist(
        values,
        bins=50,
        range=(min_val, max_val),
        edgecolor="black",
        alpha=0.6,
        label=labels,
    )
    ax.set_xlabel("Nanoseconds per operation")
    ax.set_ylabel("Frequency")
    ax.set_yscale("log")
    ax.set_title("Order Book Operation Latency Distribution")
    ax.legend()

    plt.tight_layout()
    plt.savefig(fn.with_suffix(".png"), dpi=150)
    print(f"Saved to {fn.with_suffix('.png')}")

    # Print summary stats
    print("\nSummary statistics (nanoseconds):")
    for label, vals in categories:
        vals_sorted = sorted(vals)
        n = len(vals)
        mean = sum(vals) / n
        median = vals_sorted[n // 2]
        p95 = vals_sorted[int(n * 0.95)]
        p99 = vals_sorted[int(n * 0.99)]
        print(f"  {label:15s}: n={n:>8,}  mean={mean:>8,.0f}  median={median:>6,}  p95={p95:>6,}  p99={p99:>6,}")


if __name__ == "__main__":
    assert len(sys.argv) > 1, "missing argument: results csv file"
    results = Path(sys.argv[1])
    assert results.is_file(), f"{results} not found"

    main(results)
