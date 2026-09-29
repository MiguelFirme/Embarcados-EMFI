"""Small line protocol client; transport can later be replaced independently."""

import time

import serial
from serial.tools import list_ports


class ProtocolError(RuntimeError):
    pass


def available_ports():
    return [(item.device, item.description) for item in list_ports.comports()]


class PicoClient:
    def __init__(self, port, baudrate=115200, timeout=2.0):
        # baudrate is nominal for USB CDC; it does not set the USB bit rate.
        self.serial = serial.Serial(port, baudrate=baudrate, timeout=timeout)

    def close(self):
        self.serial.close()

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()

    def command(self, value):
        if not value.isascii() or "\n" in value or "\r" in value:
            raise ValueError("command must be one ASCII line")
        self.serial.write((value + "\n").encode("ascii"))
        events = []
        deadline = time.monotonic() + self.serial.timeout
        while time.monotonic() < deadline:
            try:
                line = self.read_line()
            except TimeoutError:
                continue
            if line.startswith("EVENT "):
                events.append(line)
            elif line == "OK" or line.startswith("OK ") or line.startswith("STATUS "):
                return line, events
            else:
                raise ProtocolError(f"unexpected reply: {line!r}")
        raise TimeoutError(f"no complete reply to {value!r}")

    def read_line(self):
        raw = self.serial.readline()
        if not raw:
            raise TimeoutError("serial response timed out")
        line = raw.decode("ascii", errors="replace").strip()
        if line.startswith("ERROR "):
            raise ProtocolError(line)
        return line
