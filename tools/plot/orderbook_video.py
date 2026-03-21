# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "matplotlib>=3",
#     "numpy>=1",
#     "pillow>=10",
# ]
# ///
"""Create a video animation of LOBSTER order book evolution."""

import subprocess
import sys
import tempfile
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

LEVELS = 50


def parse_orderbook_row(row: str) -> tuple[list[tuple[float, int]], list[tuple[float, int]]]:
    """Parse a LOBSTER orderbook row into asks and bids."""
    values = [int(x) for x in row.strip().split(",")]

    asks = []
    bids = []

    for i in range(LEVELS):
        ask_price = values[i * 4] / 10000
        ask_size = values[i * 4 + 1]
        bid_price = values[i * 4 + 2] / 10000
        bid_size = values[i * 4 + 3]

        if ask_price < 999999:
            asks.append((ask_price, ask_size))
        if bid_price > -999999:
            bids.append((bid_price, bid_size))

    return asks, bids


def parse_message_row(row: str) -> tuple[float, int, int, int, float, int]:
    """Parse a LOBSTER message row."""
    parts = row.strip().split(",")
    timestamp = float(parts[0])
    msg_type = int(parts[1])
    order_id = int(parts[2])
    size = int(parts[3])
    price = float(parts[4]) / 10000
    direction = int(parts[5])
    return timestamp, msg_type, order_id, size, price, direction


def format_time(seconds_after_midnight: float) -> str:
    """Convert seconds after midnight to HH:MM:SS.mmm format."""
    hours = int(seconds_after_midnight // 3600)
    minutes = int((seconds_after_midnight % 3600) // 60)
    secs = seconds_after_midnight % 60
    return f"{hours:02d}:{minutes:02d}:{secs:06.3f}"


def render_frame(
    asks: list[tuple[float, int]],
    bids: list[tuple[float, int]],
    timestamp: float,
    frame_path: Path,
    price_min: float,
    price_max: float,
    vol_max: int,
):
    """Render a single frame with depth chart and level bars."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # Sort
    asks = sorted(asks, key=lambda x: x[0])
    bids = sorted(bids, key=lambda x: x[0], reverse=True)

    if not asks or not bids:
        plt.close()
        return

    best_bid = max(b[0] for b in bids)
    best_ask = min(a[0] for a in asks)
    spread = best_ask - best_bid

    # === Left: Depth chart ===
    ask_prices = [a[0] for a in asks]
    ask_cum_vol = np.cumsum([a[1] for a in asks])

    bid_prices = [b[0] for b in bids]
    bid_cum_vol = np.cumsum([b[1] for b in bids])
    bid_prices = bid_prices[::-1]
    bid_cum_vol = bid_cum_vol[::-1]

    ax1.fill_between(ask_prices, ask_cum_vol, alpha=0.4, color="red", step="pre")
    ax1.step(ask_prices, ask_cum_vol, color="red", where="pre", linewidth=1.5)
    ax1.fill_between(bid_prices, bid_cum_vol, alpha=0.4, color="green", step="post")
    ax1.step(bid_prices, bid_cum_vol, color="green", where="post", linewidth=1.5)

    ax1.axvline(best_bid, color="green", linestyle="--", alpha=0.7, linewidth=1)
    ax1.axvline(best_ask, color="red", linestyle="--", alpha=0.7, linewidth=1)

    ax1.set_xlabel("Price ($)", fontsize=11)
    ax1.set_ylabel("Cumulative Volume", fontsize=11)
    ax1.set_title(f"Depth Chart", fontsize=12)
    ax1.set_xlim(price_min, price_max)
    ax1.set_ylim(0, vol_max)
    ax1.grid(True, alpha=0.3)

    # === Right: Level bars (top 15) ===
    top_asks = sorted(asks, key=lambda x: x[0])[:15]
    top_bids = sorted(bids, key=lambda x: x[0], reverse=True)[:15]

    bar_width = 0.008
    for price, vol in top_bids:
        ax2.bar(price, vol, width=bar_width, color="green", alpha=0.7)
    for price, vol in top_asks:
        ax2.bar(price, vol, width=bar_width, color="red", alpha=0.7)

    ax2.axvline(best_bid, color="green", linestyle="--", alpha=0.5)
    ax2.axvline(best_ask, color="red", linestyle="--", alpha=0.5)

    ax2.set_xlabel("Price ($)", fontsize=11)
    ax2.set_ylabel("Volume", fontsize=11)
    ax2.set_title(f"Top 15 Levels", fontsize=12)
    ax2.set_xlim(price_min, price_max)
    ax2.grid(True, alpha=0.3, axis="y")

    # Main title with time and spread
    fig.suptitle(
        f"SPY Order Book | {format_time(timestamp)} | "
        f"Bid: ${best_bid:.2f} | Ask: ${best_ask:.2f} | Spread: ${spread:.4f}",
        fontsize=14,
        fontweight="bold",
    )

    plt.tight_layout()
    plt.savefig(frame_path, dpi=100)
    plt.close()


def main():
    data_dir = Path(__file__).parent.parent.parent / "data" / "lobster"

    orderbook_file = data_dir / "SPY_2012-06-21_34200000_37800000_orderbook_50.csv"
    message_file = data_dir / "SPY_2012-06-21_34200000_37800000_message_50.csv"

    if not orderbook_file.exists():
        print(f"Orderbook file not found: {orderbook_file}")
        sys.exit(1)

    # Count lines
    print("Counting lines...")
    with open(orderbook_file) as f:
        total_lines = sum(1 for _ in f)
    print(f"Total snapshots: {total_lines:,}")

    # Parameters
    target_frames = 600  # 20 seconds at 30fps
    sample_interval = max(1, total_lines // target_frames)
    fps = 30

    print(f"Sampling every {sample_interval:,} rows for ~{target_frames} frames")

    # First pass: determine price range and max volume for consistent axes
    print("Determining axis ranges...")
    price_samples = []
    vol_samples = []

    with open(orderbook_file) as ob_f, open(message_file) as msg_f:
        for i, (ob_line, msg_line) in enumerate(zip(ob_f, msg_f)):
            if i % sample_interval != 0:
                continue
            asks, bids = parse_orderbook_row(ob_line)
            if asks and bids:
                best_bid = max(b[0] for b in bids)
                best_ask = min(a[0] for a in asks)
                mid = (best_bid + best_ask) / 2
                price_samples.append(mid)

                # Cumulative volume within range
                asks_sorted = sorted(asks, key=lambda x: x[0])
                bids_sorted = sorted(bids, key=lambda x: x[0], reverse=True)
                vol_samples.append(sum(a[1] for a in asks_sorted[:20]))
                vol_samples.append(sum(b[1] for b in bids_sorted[:20]))

    price_mid = np.median(price_samples)
    price_range = 0.40  # $0.40 on each side
    price_min = price_mid - price_range
    price_max = price_mid + price_range
    vol_max = int(np.percentile(vol_samples, 95) * 1.2)

    print(f"Price range: ${price_min:.2f} - ${price_max:.2f}")
    print(f"Volume max: {vol_max:,}")

    # Render frames
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)
        print(f"Rendering frames to {tmpdir}...")

        frame_num = 0
        with open(orderbook_file) as ob_f, open(message_file) as msg_f:
            for i, (ob_line, msg_line) in enumerate(zip(ob_f, msg_f)):
                if i % sample_interval != 0:
                    continue

                asks, bids = parse_orderbook_row(ob_line)
                timestamp, *_ = parse_message_row(msg_line)

                if not asks or not bids:
                    continue

                frame_path = tmpdir / f"frame_{frame_num:05d}.png"
                render_frame(asks, bids, timestamp, frame_path, price_min, price_max, vol_max)

                frame_num += 1
                if frame_num % 50 == 0:
                    print(f"  Rendered {frame_num} frames...")

        print(f"Total frames rendered: {frame_num}")

        # Create video/gif
        output_dir = Path(__file__).parent.parent.parent

        # Try ffmpeg first for mp4
        mp4_path = output_dir / "orderbook_animation.mp4"
        ffmpeg_cmd = [
            "ffmpeg",
            "-y",
            "-framerate", str(fps),
            "-i", str(tmpdir / "frame_%05d.png"),
            "-c:v", "libx264",
            "-pix_fmt", "yuv420p",
            "-crf", "23",
            str(mp4_path),
        ]

        try:
            result = subprocess.run(ffmpeg_cmd, capture_output=True, text=True)
            if result.returncode == 0:
                print(f"Video saved to: {mp4_path}")
                print(f"Duration: {frame_num / fps:.1f} seconds at {fps} fps")
                return
        except FileNotFoundError:
            print("ffmpeg not found, creating GIF with pillow...")

        # Fallback: create GIF with pillow
        from PIL import Image

        gif_path = output_dir / "orderbook_animation.gif"
        frames = []

        frame_files = sorted(tmpdir.glob("frame_*.png"))
        print(f"Loading {len(frame_files)} frames for GIF...")

        for i, frame_file in enumerate(frame_files):
            img = Image.open(frame_file)
            # Reduce size for GIF
            img = img.resize((img.width // 2, img.height // 2), Image.Resampling.LANCZOS)
            frames.append(img)
            if (i + 1) % 100 == 0:
                print(f"  Loaded {i + 1} frames...")

        print(f"Saving GIF to {gif_path}...")
        frames[0].save(
            gif_path,
            save_all=True,
            append_images=frames[1:],
            duration=1000 // fps,  # ms per frame
            loop=0,
        )

        print(f"GIF saved to: {gif_path}")
        print(f"Duration: {frame_num / fps:.1f} seconds at {fps} fps")


if __name__ == "__main__":
    main()
