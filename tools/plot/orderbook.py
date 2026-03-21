# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "matplotlib>=3",
#     "numpy>=1",
# ]
# ///
"""Visualize LOBSTER order book data as a depth chart."""

import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

LEVELS = 50


def parse_orderbook_row(row: str) -> tuple[list[tuple[float, int]], list[tuple[float, int]]]:
    """Parse a LOBSTER orderbook row into asks and bids.

    Returns:
        (asks, bids) where each is a list of (price, volume) tuples.
        Prices are converted from LOBSTER format (price * 10000) to dollars.
    """
    values = [int(x) for x in row.strip().split(",")]

    asks = []
    bids = []

    for i in range(LEVELS):
        ask_price = values[i * 4] / 10000
        ask_size = values[i * 4 + 1]
        bid_price = values[i * 4 + 2] / 10000
        bid_size = values[i * 4 + 3]

        # Skip dummy entries (empty levels)
        if ask_price < 999999:
            asks.append((ask_price, ask_size))
        if bid_price > -999999:
            bids.append((bid_price, bid_size))

    return asks, bids


def load_orderbook_snapshot(filepath: Path, snapshot_idx: int = 0) -> tuple[list, list]:
    """Load a specific snapshot from the orderbook file."""
    with open(filepath) as f:
        for i, line in enumerate(f):
            if i == snapshot_idx:
                return parse_orderbook_row(line)
    raise ValueError(f"Snapshot {snapshot_idx} not found")


def plot_depth_chart(asks: list[tuple[float, int]], bids: list[tuple[float, int]], output: Path):
    """Create a depth chart visualization."""
    fig, ax = plt.subplots(figsize=(12, 6))

    # Sort asks ascending, bids descending
    asks = sorted(asks, key=lambda x: x[0])
    bids = sorted(bids, key=lambda x: x[0], reverse=True)

    # Calculate cumulative volumes
    ask_prices = [a[0] for a in asks]
    ask_cum_vol = np.cumsum([a[1] for a in asks])

    bid_prices = [b[0] for b in bids]
    bid_cum_vol = np.cumsum([b[1] for b in bids])

    # Reverse bids for proper display (best bid on right side of bid curve)
    bid_prices = bid_prices[::-1]
    bid_cum_vol = bid_cum_vol[::-1]

    # Plot
    ax.fill_between(ask_prices, ask_cum_vol, alpha=0.4, color="red", label="Asks", step="pre")
    ax.step(ask_prices, ask_cum_vol, color="red", where="pre", linewidth=1.5)

    ax.fill_between(bid_prices, bid_cum_vol, alpha=0.4, color="green", label="Bids", step="post")
    ax.step(bid_prices, bid_cum_vol, color="green", where="post", linewidth=1.5)

    # Mark best bid/ask
    best_bid = max(b[0] for b in bids)
    best_ask = min(a[0] for a in asks)
    spread = best_ask - best_bid

    ax.axvline(best_bid, color="green", linestyle="--", alpha=0.7, linewidth=1)
    ax.axvline(best_ask, color="red", linestyle="--", alpha=0.7, linewidth=1)

    # Labels and formatting
    ax.set_xlabel("Price ($)", fontsize=12)
    ax.set_ylabel("Cumulative Volume (shares)", fontsize=12)
    ax.set_title(f"SPY Order Book Depth\nBest Bid: ${best_bid:.2f} | Best Ask: ${best_ask:.2f} | Spread: ${spread:.4f}", fontsize=14)
    ax.legend(loc="upper right")
    ax.grid(True, alpha=0.3)

    # Focus on the interesting part (near the spread)
    mid = (best_bid + best_ask) / 2
    price_range = 0.5  # $0.50 on each side
    ax.set_xlim(mid - price_range, mid + price_range)

    plt.tight_layout()
    plt.savefig(output, dpi=150)
    plt.close()

    print(f"Saved depth chart to {output}")


def plot_level_bars(asks: list[tuple[float, int]], bids: list[tuple[float, int]], output: Path, levels: int = 10):
    """Create a bar chart showing volume at each price level."""
    fig, ax = plt.subplots(figsize=(12, 6))

    # Take top N levels
    asks = sorted(asks, key=lambda x: x[0])[:levels]
    bids = sorted(bids, key=lambda x: x[0], reverse=True)[:levels]

    # Combine and sort all prices for x-axis
    all_prices = sorted(set([b[0] for b in bids] + [a[0] for a in asks]))

    # Create price -> volume mappings
    bid_vol = {b[0]: b[1] for b in bids}
    ask_vol = {a[0]: a[1] for a in asks}

    bar_width = 0.008
    for price in all_prices:
        if price in bid_vol:
            ax.bar(price, bid_vol[price], width=bar_width, color="green", alpha=0.7)
        if price in ask_vol:
            ax.bar(price, ask_vol[price], width=bar_width, color="red", alpha=0.7)

    # Labels
    best_bid = max(b[0] for b in bids)
    best_ask = min(a[0] for a in asks)

    ax.axvline(best_bid, color="green", linestyle="--", alpha=0.5, label=f"Best Bid ${best_bid:.2f}")
    ax.axvline(best_ask, color="red", linestyle="--", alpha=0.5, label=f"Best Ask ${best_ask:.2f}")

    ax.set_xlabel("Price ($)", fontsize=12)
    ax.set_ylabel("Volume (shares)", fontsize=12)
    ax.set_title(f"SPY Order Book - Top {levels} Levels", fontsize=14)
    ax.legend()
    ax.grid(True, alpha=0.3, axis="y")

    plt.tight_layout()
    plt.savefig(output, dpi=150)
    plt.close()

    print(f"Saved level bars to {output}")


def main():
    # Find the orderbook file
    data_dir = Path(__file__).parent.parent.parent / "data" / "lobster"
    orderbook_files = list(data_dir.glob("*_orderbook_*.csv"))

    if not orderbook_files:
        print("No orderbook files found in data/lobster/")
        sys.exit(1)

    orderbook_file = orderbook_files[0]
    print(f"Loading: {orderbook_file.name}")

    # Load first snapshot
    asks, bids = load_orderbook_snapshot(orderbook_file, snapshot_idx=0)

    print(f"Loaded {len(asks)} ask levels, {len(bids)} bid levels")
    print(f"Best ask: ${min(a[0] for a in asks):.2f}")
    print(f"Best bid: ${max(b[0] for b in bids):.2f}")

    # Generate plots
    output_dir = Path(__file__).parent.parent.parent
    plot_depth_chart(asks, bids, output_dir / "orderbook_depth.png")
    plot_level_bars(asks, bids, output_dir / "orderbook_levels.png", levels=15)


if __name__ == "__main__":
    main()
