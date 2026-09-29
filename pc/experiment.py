"""Result parsing, independent of the serial transport and CSV storage."""

from dataclasses import dataclass
from datetime import datetime, timezone


@dataclass(frozen=True)
class Result:
    timestamp: str
    experiment_id: int
    delay_ns: int
    pulse_width_ns: int
    result: str
    target_response: str
    elapsed_us: int


def parse_result(line):
    parts = line.split(" ")
    if len(parts) != 7 or parts[0] != "RESULT":
        raise ValueError(f"invalid result line: {line!r}")
    _, experiment_id, delay_us, width_us, result, elapsed_us, response = parts
    if result not in {"OK", "FAULT", "TIMEOUT", "RESET", "UNKNOWN"}:
        raise ValueError(f"invalid result class: {result!r}")
    return Result(
        datetime.now(timezone.utc).isoformat(),
        int(experiment_id),
        int(delay_us) * 1000,
        int(width_us) * 1000,
        result,
        response,
        int(elapsed_us),
    )
