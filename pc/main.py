"""CLI for manual pulses, one target trial, and automatic delay sweeps."""

import argparse
import sys

import serial

from experiment import parse_result
from logger import append_result
from serial_interface import PicoClient, ProtocolError, available_ports


def add_trial_options(parser):
    parser.add_argument("--width", type=int, default=10, help="pulse width in us")
    parser.add_argument("--trigger-timeout", type=int, default=500, help="ms")
    parser.add_argument("--result-timeout", type=int, default=500, help="ms")
    parser.add_argument("--reset-target", action="store_true")
    parser.add_argument("--output", default="results.csv", help="CSV path")


def configure_trial(client, args):
    if not 1 <= args.width <= 1_000_000:
        raise ValueError("--width must be 1..1000000 us")
    for name, value in (("TRIGGER_TIMEOUT_MS", args.trigger_timeout),
                        ("RESULT_TIMEOUT_MS", args.result_timeout)):
        if not 1 <= value <= 60_000:
            raise ValueError(f"{name} must be 1..60000 ms")
    for command in (
        f"SET WIDTH_US {args.width}",
        f"SET TRIGGER_TIMEOUT_MS {args.trigger_timeout}",
        f"SET RESULT_TIMEOUT_MS {args.result_timeout}",
        f"SET RESET_TARGET {int(args.reset_target)}",
    ):
        client.command(command)
    client.serial.timeout = max(2.0, (args.trigger_timeout + args.result_timeout) / 1000 + 1)


def collect(client, output, sweep):
    count = 0
    while True:
        line = client.read_line()
        if line.startswith("EVENT "):
            print(line)
        elif line.startswith("RESULT "):
            result = parse_result(line)
            append_result(output, result)
            count += 1
            print(line)
            if not sweep:
                return count
        elif line == "SWEEP COMPLETE" and sweep:
            print(line)
            return count
        else:
            raise ProtocolError(f"unexpected stream line: {line!r}")


def main(argv=None):
    parser = argparse.ArgumentParser(description="RP2040 GPIO fault-injection test rig")
    parser.add_argument("--port", help="USB CDC port; auto-selects if exactly one exists")
    sub = parser.add_subparsers(dest="action", required=True)
    for name in ("ports", "ping", "status", "arm", "disarm", "pulse", "reset-target"):
        sub.add_parser(name)
    width = sub.add_parser("set-width", help="set manual pulse width in us")
    width.add_argument("microseconds", type=int)
    run = sub.add_parser("run", help="one triggered target trial")
    run.add_argument("--delay", type=int, default=0, help="trigger-to-pulse delay in us")
    add_trial_options(run)
    sweep = sub.add_parser("sweep", help="sweep trigger-to-pulse delay in us")
    for name in ("start", "end", "step", "repetitions"):
        sweep.add_argument(f"--{name}", type=int, required=True)
    add_trial_options(sweep)
    args = parser.parse_args(argv)

    if args.action == "ports":
        for device, description in available_ports():
            print(f"{device}: {description}")
        return 0
    ports = available_ports()
    if not args.port:
        if len(ports) != 1:
            parser.error("specify --port when zero or multiple serial ports are found")
        args.port = ports[0][0]

    commands = {
        "ping": "PING", "status": "STATUS", "arm": "ARM",
        "disarm": "DISARM", "pulse": "PULSE", "reset-target": "RESET TARGET",
    }
    try:
        with PicoClient(args.port) as client:
            if args.action in ("run", "sweep"):
                configure_trial(client, args)
                if args.action == "run":
                    if not 0 <= args.delay <= 1_000_000:
                        raise ValueError("--delay must be 0..1000000 us")
                    client.command(f"SET DELAY_US {args.delay}")
                    command = "RUN"
                else:
                    if (args.step <= 0 or args.repetitions <= 0 or
                            args.start < 0 or args.end < args.start or
                            args.end > 1_000_000 or
                            ((args.end - args.start) // args.step + 1) * args.repetitions > 10_000):
                        raise ValueError("invalid sweep range or more than 10000 trials")
                    command = (f"SWEEP START {args.start} {args.end} "
                               f"{args.step} {args.repetitions}")
                reply, events = client.command(command)
                print(reply)
                for event in events:
                    print(event)
                count = collect(client, args.output, args.action == "sweep")
                print(f"saved {count} result(s) to {args.output}")
            else:
                if args.action == "set-width":
                    if not 1 <= args.microseconds <= 1_000_000:
                        raise ValueError("width must be 1..1000000 us")
                    command = f"SET WIDTH_US {args.microseconds}"
                else:
                    command = commands[args.action]
                reply, events = client.command(command)
                for event in events:
                    print(event)
                print(reply)
        return 0
    except (serial.SerialException, ProtocolError, TimeoutError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
