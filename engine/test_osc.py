"""Tests for the Python OSC codec.

Runnable with the stdlib only:  python -m unittest engine.test_osc
(or `python -m unittest` discovery from the engine/ directory).

The `pin_*` tests assert the *exact same canonical bytes* as the C++ tests in
tests/osc_test.cpp. As long as both suites pass, the two codecs are wire-compatible.
"""

import struct
import unittest

import osc


class TestOscCodec(unittest.TestCase):
    def test_roundtrip_control(self):
        data = osc.encode("/channel/3/level", [0.75])
        addr, args = osc.decode(data)
        self.assertEqual(addr, "/channel/3/level")
        # 0.75 is exactly representable in float32, so it survives the round trip.
        self.assertEqual(args, [0.75])

    def test_roundtrip_meters(self):
        levels = [i / 8.0 for i in range(8)]
        addr, args = osc.decode(osc.encode("/meters", levels))
        self.assertEqual(addr, "/meters")
        self.assertEqual(args, levels)

    def test_roundtrip_mixed(self):
        addr, args = osc.decode(osc.encode("/say", [7, "hi", 1.5]))
        self.assertEqual(addr, "/say")
        self.assertEqual(args, [7, "hi", 1.5])

    def test_pin_empty_message_bytes(self):
        # "/a" -> "/a\0\0", tag "," -> ",\0\0\0"; mirrors test_pin_empty_message_bytes.
        self.assertEqual(osc.encode("/a"), b"\x2f\x61\x00\x00\x2c\x00\x00\x00")

    def test_pin_float_bigendian(self):
        # 440.0f == IEEE-754 0x43DC0000, big-endian on the wire (mirrors C++ test).
        self.assertEqual(osc.encode("/f", [440.0])[-4:], b"\x43\xdc\x00\x00")

    def test_decode_rejects_garbage(self):
        with self.assertRaises(ValueError):
            osc.decode(b"not-osc")          # no null terminator
        with self.assertRaises(ValueError):
            osc.decode(osc.encode("/x", [1])[:-2])  # truncated int arg

    def test_bool_rejected(self):
        with self.assertRaises(TypeError):
            osc.encode("/x", [True])


if __name__ == "__main__":
    unittest.main()
