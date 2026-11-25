#!/usr/bin/env python3
"""Capture the serial output of the board for N seconds.

`idf.py monitor` needs an interactive TTY, so this is the scriptable
alternative. Run it inside the ESP-IDF environment (source tools/env.sh) so
that pyserial is available.

Usage: serial_capture.py [-p PORT] [-b BAUD] [-t SECONDS] [-o FILE] [--reset]
"""
import argparse
import sys
import time

import serial


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("-p", "--port", default="/dev/ttyUSB0")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("-t", "--seconds", type=float, default=30.0)
    ap.add_argument("-o", "--output", help="also write the capture to this file")
    ap.add_argument("--reset", action="store_true", help="pulse RTS (EN) to reboot the board first")
    args = ap.parse_args()

    out = open(args.output, "w", encoding="utf-8") if args.output else None
    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        if args.reset:
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.rts = False
        end = time.monotonic() + args.seconds
        buf = b""
        while time.monotonic() < end:
            buf += ser.read(4096)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", errors="replace").rstrip("\r")
                print(text, flush=True)
                if out:
                    out.write(text + "\n")
                    out.flush()  # a long capture can be read while it runs
    if out:
        out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
