#!/usr/bin/env python3
"""
plot_cache.py — Visualize cache hierarchy benchmark results
Aditi Shankar | BITS Pilani Goa

Usage:
    ./cache_bench | tee results.txt
    python3 plot_cache.py results.txt
"""

import sys
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np


# ── Step 1: Parse the results file ───────────────────────────────────────────
#
# Your C program prints a CSV section that looks like:
#
#   size_bytes,seq_ns,rnd_ns
#   4096,1.021,1.843
#   8192,1.019,1.901
#   ...
#
# This function finds that section and extracts the numbers.

def parse_results(filename):
    sizes = []   # buffer size in bytes
    seq   = []   # sequential latency in ns
    rnd   = []   # random latency in ns

    in_csv = False  # flag: have we reached the CSV section yet?

    with open(filename) as f:
        for line in f:
            line = line.strip()

            # Look for the header line that marks start of CSV
            if line == "size_bytes,seq_ns,rnd_ns":
                in_csv = True
                continue  # skip the header line itself

            # Once we're in CSV section, parse each data line
            if in_csv and line:
                parts = line.split(",")
                if len(parts) == 3:
                    try:
                        sizes.append(int(parts[0]))      # bytes
                        seq.append(float(parts[1]))      # ns
                        rnd.append(float(parts[2]))      # ns
                    except ValueError:
                        break  # hit something that isn't data, stop

    return sizes, seq, rnd


# ── Step 2: Convert bytes to human-readable labels ───────────────────────────
#
# We want axis labels like "4 KB", "256 KB", "4 MB" instead of "4096", etc.

def human_label(bytes_val):
    if bytes_val >= 1024 * 1024:
        return f"{bytes_val // (1024*1024)} MB"
    return f"{bytes_val // 1024} KB"


# ── Step 3: Draw the graphs ───────────────────────────────────────────────────

def plot(sizes, seq, rnd):
    # Convert to numpy arrays for easier math
    sizes = np.array(sizes)
    seq   = np.array(seq)
    rnd   = np.array(rnd)

    # x axis: one tick per buffer size
    x      = np.arange(len(sizes))
    labels = [human_label(s) for s in sizes]

    # ── Figure setup ─────────────────────────────────────────────────────────
    fig, axes = plt.subplots(
        2, 1,                    # 2 rows, 1 column = two stacked graphs
        figsize=(13, 9),         # width x height in inches
        facecolor='#F8F9FA'      # light grey background
    )
    fig.suptitle(
        "Cache Hierarchy Performance Profile\n"
        "Aditi Shankar  |  BITS Pilani, Goa Campus",
        fontsize=14, fontweight='bold', y=0.98
    )

    # ── Graph 1: Latency vs Working Set Size ──────────────────────────────────
    ax1 = axes[0]
    ax1.set_facecolor('#FFFFFF')

    # Plot sequential — blue line with circle markers
    ax1.plot(
        x, seq,
        color='#2196F3', linewidth=2.5, marker='o',
        markersize=7, label='Sequential (prefetcher active)'
    )

    # Plot random — red line with square markers
    ax1.plot(
        x, rnd,
        color='#F44336', linewidth=2.5, marker='s',
        markersize=7, label='Random / Pointer-chase (prefetcher blind)'
    )

    # Log scale on Y axis — otherwise the DRAM values dwarf everything else
    # and you can't see the L1/L2 differences at all
    ax1.set_yscale('log')
    ax1.set_ylabel("Latency (ns per access) — log scale", fontsize=11)
    ax1.set_title("Memory Access Latency vs Working Set Size", fontsize=12)
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels, rotation=45, ha='right', fontsize=9)
    ax1.legend(fontsize=10)
    ax1.grid(True, alpha=0.3, which='both')

    # ── Shade cache regions ───────────────────────────────────────────────────
    # These are approximate boundaries — your actual boundaries
    # will be visible as steps in the random line
    #
    # Adjust these indices if your sizes[] array is different:
    # sizes: [4K,8K,16K, 64K,128K,256K, 1M,4M,8M,16M, 64M,128M,256M]
    # idx:   [ 0, 1,  2,   3,  4,   5,  6, 7, 8,  9,  10,  11,  12]

    region_spans = [
        (-.5,  2.5, '#4CAF50', 'L1'),    # indices 0–2:  4KB–16KB
        (2.5,  5.5, '#2196F3', 'L2'),    # indices 3–5:  64KB–256KB
        (5.5,  9.5, '#FF9800', 'L3'),    # indices 6–9:  1MB–16MB
        (9.5, 12.5, '#F44336', 'DRAM'),  # indices 10–12: 64MB–256MB
    ]

    ymin, ymax = ax1.get_ylim()
    for x_start, x_end, color, label_text in region_spans:
        # Shaded band
        ax1.axvspan(x_start, x_end, alpha=0.07, color=color)
        # Region label at top of band
        mid = (x_start + x_end) / 2
        ax1.text(
            mid, ymax * 0.7, label_text,
            ha='center', va='top', fontsize=9,
            color=color, fontweight='bold', alpha=0.8
        )

    # Vertical dashed lines at boundaries
    for boundary_x, boundary_label in [(2.5,'L1→L2'), (5.5,'L2→L3'), (9.5,'L3→DRAM')]:
        ax1.axvline(boundary_x, color='gray', linestyle='--',
                    linewidth=1.0, alpha=0.5)

    # ── Graph 2: Prefetcher effectiveness ratio ───────────────────────────────
    ax2 = axes[1]
    ax2.set_facecolor('#FFFFFF')

    # ratio = random / sequential
    # When both fast (L1): ratio ~1-2x  → prefetcher barely needed
    # When random slow (DRAM): ratio ~50-100x → prefetcher essential
    ratio = rnd / seq

    # Bar chart — one bar per buffer size
    bar_colors = []
    for r in ratio:
        if   r <  5:  bar_colors.append('#4CAF50')   # green  = L1
        elif r < 15:  bar_colors.append('#2196F3')   # blue   = L2
        elif r < 50:  bar_colors.append('#FF9800')   # orange = L3
        else:         bar_colors.append('#F44336')   # red    = DRAM

    bars = ax2.bar(x, ratio, color=bar_colors, alpha=0.8, edgecolor='white')

    # Label each bar with its ratio value
    for bar, val in zip(bars, ratio):
        ax2.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.3,
            f'{val:.1f}×',
            ha='center', va='bottom', fontsize=8, fontweight='bold'
        )

    ax2.set_xticks(x)
    ax2.set_xticklabels(labels, rotation=45, ha='right', fontsize=9)
    ax2.set_ylabel("Random ÷ Sequential Latency", fontsize=11)
    ax2.set_title(
        "Prefetcher Effectiveness  "
        "(higher ratio = prefetcher less helpful = deeper in memory hierarchy)",
        fontsize=11
    )
    ax2.axhline(1.0, color='black', linewidth=0.8, linestyle='--', alpha=0.4)
    ax2.grid(True, alpha=0.3, axis='y')

    # ── Finish up ─────────────────────────────────────────────────────────────
    plt.tight_layout(rect=[0, 0, 1, 0.96])  # leave room for suptitle

    # Save as PNG
    output_file = "cache_profile.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"\nGraph saved: {output_file}")

    # Show interactively if display is available
    try:
        plt.show()
    except Exception:
        print("(No display available — graph saved to file only)")


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    # Get filename from command line argument
    if len(sys.argv) < 2:
        print("Usage: python3 plot_cache.py results.txt")
        print("       (run ./cache_bench | tee results.txt first)")
        sys.exit(1)

    filename = sys.argv[1]

    # Parse
    sizes, seq, rnd = parse_results(filename)

    if not sizes:
        print("Error: no CSV data found in the file.")
        print("Make sure your cache_bench ran successfully and produced CSV output.")
        sys.exit(1)

    print(f"Loaded {len(sizes)} data points from {filename}")

    # Print a quick summary to terminal
    print(f"\n{'Size':<10}  {'Sequential(ns)':<16}  {'Random(ns)':<16}  Region")
    print(f"{'----':<10}  {'------------':<16}  {'----------':<16}  ------")
    for s, sq, rd in zip(sizes, seq, rnd):
        region = ('L1' if rd < 5 else
                  'L2' if rd < 20 else
                  'L3' if rd < 80 else 'DRAM')
        print(f"{human_label(s):<10}  {sq:<16.3f}  {rd:<16.3f}  {region}")

    # Plot
    plot(sizes, seq, rnd)


if __name__ == "__main__":
    main()