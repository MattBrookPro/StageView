#!/usr/bin/env python3
"""Probe the StageView mock engine from the command line.

A tiny OSC/UDP client used during development to confirm the engine is alive
before bringing the GUI surface into the picture: it subscribes, prints the state
snapshot, streams a few meter frames, optionally pushes a control change, and
checks the meter responds. Handy for demos and as a smoke test.

Usage:
  python tools/probe.py                 # snapshot + 1s of meters
  python tools/probe.py --set 0 level 0 # also mute channel 0's level, watch it drop
"""

from __future__ import annotations

import argparse
import os
import socket
import sys
import time

# Reuse the engine's own codec so the probe can't drift from the real wire format.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "engine"))
import osc  # noqa: E402


def main() -> int:
    p = argparse.ArgumentParser(description="Probe the StageView mock engine.")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=9000)
    p.add_argument("--seconds", type=float, default=1.0)
    p.add_argument("--set", nargs=3, metavar=("CHANNEL", "PARAM", "VALUE"),
                   help="send /channel/<CHANNEL>/<PARAM> <VALUE> after subscribing")
    args = p.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    engine = (args.host, args.port)

    sock.sendto(osc.encode("/subscribe"), engine)

    channels: dict[int, dict] = {}
    meter_frames = 0
    deadline = time.monotonic() + args.seconds + 0.5
    sent_control = False

    while time.monotonic() < deadline:
        try:
            data, _ = sock.recvfrom(4096)
        except socket.timeout:
            print("no response from engine - is it running?", file=sys.stderr)
            return 1
        try:
            address, payload = osc.decode(data)
        except ValueError:
            continue

        if address == "/stage/channels":
            print(f"engine reports {payload[0]} channels")
        elif address.startswith("/channel/"):
            # param path may be multi-segment, e.g. /channel/0/eq/low
            parts = address.split("/")  # ['', 'channel', '<n>', 'param', ...]
            if len(parts) >= 4 and payload:
                idx = int(parts[2])
                param = "/".join(parts[3:])
                channels.setdefault(idx, {})[param] = payload[0]
        elif address == "/meters":
            meter_frames += 1
            if meter_frames == 1:
                names = [channels.get(i, {}).get("name", f"ch{i}")
                         for i in range(len(payload))]
                print("channels:", ", ".join(names))
            bars = " ".join(f"{v:4.2f}" for v in payload)
            print(f"meters: {bars}")
            # Once we've seen the layout, optionally push one control change.
            if args.set and not sent_control:
                ch, par, val = args.set
                fval = float(val)
                osc_val = int(fval) if par == "mute" else fval
                sock.sendto(osc.encode(f"/channel/{ch}/{par}", [osc_val]), engine)
                print(f">> set /channel/{ch}/{par} = {osc_val}")
                sent_control = True

    sock.sendto(osc.encode("/unsubscribe"), engine)
    print(f"\nreceived {meter_frames} meter frames over ~{args.seconds:g}s")
    return 0 if meter_frames > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
