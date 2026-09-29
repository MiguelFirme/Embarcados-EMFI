"""Plot artificial FAULT frequency for each requested delay."""

import csv
from collections import defaultdict
from pathlib import Path


def plot_fault_rate(csv_path, output_path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    counts = defaultdict(lambda: [0, 0])
    with Path(csv_path).open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            delay_us = int(row["delay_ns"]) / 1000
            counts[delay_us][0] += 1
            counts[delay_us][1] += row["result"] == "FAULT"
    if not counts:
        raise ValueError("CSV has no results")
    delays = sorted(counts)
    rates = [100 * counts[delay][1] / counts[delay][0] for delay in delays]
    fig, ax = plt.subplots(figsize=(8, 4))
    ax.plot(delays, rates, marker="o")
    ax.set(xlabel="Delay solicitado (µs)", ylabel="FAULT / tentativas (%)",
           title="Taxa de falha por atraso")
    ax.set_ylim(0, 100)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
