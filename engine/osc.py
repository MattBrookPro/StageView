"""Minimal OSC 1.0 codec - the Python mirror of core/Osc.{h,cpp}.

The mock engine and the C++ surface use byte-for-byte the same wire format, so
they interoperate by construction. The unit tests on both sides pin the same
canonical bytes (e.g. 440.0f -> 43 DC 00 00), which is what actually guarantees
interop without needing a live cross-process handshake in CI.

Supported argument types: int32 ('i'), float32 ('f'), string ('s').
"""

from __future__ import annotations

import struct


def _pad4(b: bytes) -> bytes:
    """Pad a block with nulls up to the next 4-byte boundary."""
    return b + b"\x00" * ((4 - len(b) % 4) % 4)


def _osc_string(s: str) -> bytes:
    # OSC strings are null-terminated, then padded - so always add the null first.
    return _pad4(s.encode("utf-8") + b"\x00")


def encode(address: str, args=()) -> bytes:
    """Encode an address + args into an OSC datagram (bytes)."""
    tags = ","
    data = b""
    for a in args:
        # bool is a subclass of int in Python; reject it explicitly so a stray
        # True/False can't silently serialise as an int32.
        if isinstance(a, bool):
            raise TypeError("bool is not a valid OSC argument")
        if isinstance(a, int):
            tags += "i"
            data += struct.pack(">i", a)
        elif isinstance(a, float):
            tags += "f"
            data += struct.pack(">f", a)
        elif isinstance(a, str):
            tags += "s"
            data += _osc_string(a)
        else:
            raise TypeError(f"unsupported OSC argument type: {type(a).__name__}")
    return _osc_string(address) + _osc_string(tags) + data


def _read_string(data: bytes, pos: int) -> tuple[str, int]:
    end = data.find(b"\x00", pos)
    if end < 0:
        raise ValueError("OSC string is not null-terminated")
    s = data[pos:end].decode("utf-8")
    field = (end - pos) + 1  # include the null we found
    pos = pos + field + ((4 - field % 4) % 4)
    return s, pos


def decode(data: bytes) -> tuple[str, list]:
    """Decode one OSC message -> (address, [args]). Raises ValueError if malformed."""
    pos = 0
    address, pos = _read_string(data, pos)
    if not address.startswith("/"):
        raise ValueError("OSC address must start with '/'")
    tags, pos = _read_string(data, pos)
    if not tags.startswith(","):
        raise ValueError("OSC type-tag string must start with ','")

    args: list = []
    for t in tags[1:]:
        if t == "i":
            if pos + 4 > len(data):
                raise ValueError("truncated int32 argument")
            args.append(struct.unpack(">i", data[pos:pos + 4])[0])
            pos += 4
        elif t == "f":
            if pos + 4 > len(data):
                raise ValueError("truncated float32 argument")
            args.append(struct.unpack(">f", data[pos:pos + 4])[0])
            pos += 4
        elif t == "s":
            s, pos = _read_string(data, pos)
            args.append(s)
        else:
            raise ValueError(f"unsupported OSC type tag: {t!r}")
    return address, args
