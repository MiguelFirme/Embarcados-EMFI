"""Append results to a stable, spreadsheet-friendly CSV schema."""

import csv
from dataclasses import asdict
from pathlib import Path


FIELDS = (
    "timestamp", "experiment_id", "delay_ns", "pulse_width_ns",
    "result", "target_response", "elapsed_us",
)


def append_result(path, result):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    needs_header = not path.exists() or path.stat().st_size == 0
    with path.open("a", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        if needs_header:
            writer.writeheader()
        writer.writerow(asdict(result))
