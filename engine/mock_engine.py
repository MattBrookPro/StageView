#!/usr/bin/env python3
"""StageView mock audio engine.

A stand-in for the real audio engine. It holds per-channel state (level, pan,
mute), generates believable meter data, and speaks OSC over UDP. Run it, then
launch the StageView surface - the surface subscribes and starts driving it.

Two design choices worth calling out:

  * The engine is deliberately ignorant of the stage metaphor. It only ever sees
    audio parameters (level/pan/mute) - exactly what a real engine exposes. The
    "drag a source around a stage" idea is a pure reinterpretation on the surface
    side. That keeps the protocol honest and the engine swappable for a real one.

  * The whole thing is a single-threaded ~50 Hz loop: drain inbound control, then
    advance and broadcast meters. No threads, no locks: a UDP server at this rate
    doesn't need them, and the simpler the mock, the more attention the surface's
    threading (the part that matters) gets.

Protocol:
  surface -> engine   /subscribe                       (reply-to-sender; no args)
                      /unsubscribe
                      /channel/<n>/level   <float 0..1>
                      /channel/<n>/pan     <float -1..1>
                      /channel/<n>/mute    <int 0|1>
  engine  -> surface  /stage/channels <int N>          (sent on subscribe)
                      /channel/<n>/name  <string>       (snapshot, on subscribe)
                      /channel/<n>/level <float>
                      /channel/<n>/pan   <float>
                      /channel/<n>/mute  <int>
                      /meters <float * N>               (streamed at --rate Hz)
"""

from __future__ import annotations

import argparse
import math
import random
import socket
import time

import osc

# A small band's worth of sources, so the stage has a musical identity rather
# than "Channel 1..8". Initial (level, pan) give the surface a layout to render.
INITIAL_CHANNELS = [
    # name,    level, pan
    ("Kick",   0.82, -0.15),
    ("Snare",  0.74,  0.10),
    ("Hat",    0.55,  0.40),
    ("Bass",   0.80, -0.35),
    ("Gtr",    0.62,  0.55),
    ("Keys",   0.58, -0.55),
    ("Vox",    0.86,  0.00),
    ("FX",     0.45,  0.70),
]


class Channel:
    """One audio channel: the parameters a real engine would expose, plus a meter."""

    def __init__(self, name: str, level: float, pan: float):
        self.name = name
        self.level = level   # post-fader gain, 0..1
        self.pan = pan       # -1 (L) .. +1 (R)
        self.mute = False
        self.meter = 0.0     # smoothed output level the surface displays
        # Per-channel LFO rate/phase so meters breathe independently instead of
        # pulsing in lockstep - that lockstep is the tell-tale of a fake meter.
        self._rate = random.uniform(1.5, 5.0)
        self._phase = random.uniform(0.0, math.tau)

    def advance(self, t: float) -> float:
        """Advance the meter one tick toward a level-driven target and return it."""
        if self.mute or self.level <= 0.0:
            target = 0.0
        else:
            lfo = 0.6 + 0.4 * abs(math.sin(t * self._rate + self._phase))
            target = self.level * lfo + random.uniform(-0.05, 0.08)  # sparkle
        target = max(0.0, min(1.0, target))
        # Ease toward the target so the meter has ballistics, not jitter.
        self.meter += (target - self.meter) * 0.4
        return self.meter


def _send(sock: socket.socket, addr, address: str, args=()):
    sock.sendto(osc.encode(address, list(args)), addr)


def _send_snapshot(sock: socket.socket, addr, channels: list[Channel]):
    """Tell a freshly-subscribed surface how many channels exist and their state."""
    _send(sock, addr, "/stage/channels", [len(channels)])
    for i, c in enumerate(channels):
        _send(sock, addr, f"/channel/{i}/name", [c.name])
        _send(sock, addr, f"/channel/{i}/level", [c.level])
        _send(sock, addr, f"/channel/{i}/pan", [c.pan])
        _send(sock, addr, f"/channel/{i}/mute", [1 if c.mute else 0])


def _apply_param(channel: Channel, param: str, value) -> None:
    if param == "level":
        channel.level = max(0.0, min(1.0, float(value)))
    elif param == "pan":
        channel.pan = max(-1.0, min(1.0, float(value)))
    elif param == "mute":
        channel.mute = bool(int(value))
    # Unknown params are ignored on purpose - forward-compatible with a surface
    # that sends parameters this mock doesn't model yet.


def _handle(data: bytes, addr, channels: list[Channel], subscribers: set, sock):
    try:
        address, args = osc.decode(data)
    except ValueError:
        return  # never trust the wire; drop anything malformed

    if address == "/subscribe":
        subscribers.add(addr)
        _send_snapshot(sock, addr, channels)
    elif address == "/unsubscribe":
        subscribers.discard(addr)
    elif address.startswith("/channel/"):
        # "/channel/<n>/<param>" -> ['', 'channel', '<n>', '<param>']
        parts = address.split("/")
        if len(parts) == 4 and parts[2].isdigit() and args:
            i = int(parts[2])
            if 0 <= i < len(channels):
                _apply_param(channels[i], parts[3], args[0])


def run(host: str, port: int, rate: float) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((host, port))
    sock.setblocking(False)  # we poll; the loop's sleep paces it, not the socket

    channels = [Channel(n, lvl, pan) for (n, lvl, pan) in INITIAL_CHANNELS]
    subscribers: set = set()
    period = 1.0 / rate
    start = time.monotonic()
    next_tick = start

    print(f"StageView mock engine: {len(channels)} channels on "
          f"udp://{host}:{port}, meters at {rate:g} Hz. Ctrl+C to stop.")

    try:
        while True:
            # 1) Drain every control datagram waiting in the socket buffer.
            while True:
                try:
                    data, addr = sock.recvfrom(4096)
                except BlockingIOError:
                    break
                _handle(data, addr, channels, subscribers, sock)

            # 2) On each tick, advance the meter model and broadcast to subscribers.
            now = time.monotonic()
            if now >= next_tick:
                t = now - start
                meters = [c.advance(t) for c in channels]
                if subscribers:
                    msg = osc.encode("/meters", meters)
                    for s in list(subscribers):
                        sock.sendto(msg, s)
                next_tick += period
                if now - next_tick > period:   # fell behind; resync, don't spiral
                    next_tick = now + period

            time.sleep(max(0.0, next_tick - time.monotonic()))
    except KeyboardInterrupt:
        print("\nengine stopped.")
    finally:
        sock.close()


def main() -> None:
    p = argparse.ArgumentParser(description="StageView mock audio engine (OSC/UDP).")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=9000)
    p.add_argument("--rate", type=float, default=50.0, help="meter updates per second")
    args = p.parse_args()
    run(args.host, args.port, args.rate)


if __name__ == "__main__":
    main()
