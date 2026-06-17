# Architecture

This document describes how StageView is put together and why the pieces are split the
way they are. It is the technical companion to the [README](README.md).

## Two processes, one protocol

StageView is deliberately split into two programs that only ever talk over the network:

1. The **surface**: a Qt 6 / QML and C++17 application. It owns the user interface (the
   top-down stage, draggable sources, live meters) and all of the interaction logic. It
   does no audio processing of its own.
2. The **engine**: a separate process that owns audio state and, in the real engine,
   actually plays and mixes sound. Two engines ship in this repo: a Python mock engine
   for development, tests and CI, and a real C++ / RtAudio engine that plays stems with
   per-channel DSP.

The two sides communicate with **OSC 1.0 over UDP**. Nothing else is shared: no files, no
database, no shared memory. The surface sends control messages and receives a meter feed,
and that is the entire contract. This split has three practical benefits:

- The surface can drive either engine without changing a line of code, because both speak
  the identical protocol.
- The engine can be swapped, rewritten in another language, or run on another machine, and
  the surface neither knows nor cares.
- Each side can be tested in isolation. The OSC codec, for example, is a standalone unit
  with no UI and no audio dependency.

```
+-----------------------------+        OSC over UDP        +----------------------------+
|  StageView surface (C++)     |  --- /channel/N/level -->  |  Audio engine               |
|  QML UI  +  C++ model         |                            |  (Python mock or C++ real)  |
|  network thread + UI thread   |  <-- meter feed (50 Hz) -- |  channel state + meters     |
+-----------------------------+                            +----------------------------+
```

## The OSC codec

OSC (Open Sound Control) is a small, well-defined wire format. StageView implements it by
hand rather than pulling a library, for three reasons: the format is tiny, owning it keeps
the project dependency-free in its core, and a hand-written codec is fully explainable and
unit-testable.

The format used here is the OSC 1.0 subset the app actually needs:

```
[address string]   null-terminated, padded with nulls to a multiple of 4 bytes
[type-tag string]  ',' then one tag per argument, null-terminated and padded
[arguments]        in order; int32 ('i') and float32 ('f') are big-endian, string ('s')
```

Two independent implementations exist and they are pinned to the **same canonical bytes**
by their test suites: the C++ codec in [`core/Osc.h`](core/Osc.h) and a Python mirror used
by the mock engine. Because both suites assert the exact byte layout of the same messages,
the two codecs are wire-compatible by construction rather than by hope. The companion
clients (C#, Dart, Object Pascal) each carry their own small codec held to the same layout.

The decoder is written to distrust the wire. A short or malformed datagram returns failure
and leaves the output untouched, because networking code must never trust the bytes it is
handed.

## The threading model

This is the most important part of the system and the part that takes the most care.

The engine broadcasts meter data at roughly 50 Hz. The surface renders at 60 fps. The
problem: the network read must never stall the render, and the render must never block on
the network. If meter handling happened on the UI thread, a slow or blocked socket read
would freeze the animation; if rendering touched the socket, a busy frame would drop meter
packets.

The solution is a clean thread boundary, implemented in
[`core/OscEndpoint.h`](core/OscEndpoint.h):

- The `OscEndpoint` object lives on its **own worker thread**. Every socket operation
  (bind, read, write) happens on that thread's event loop. The UI thread therefore never
  performs blocking network I/O.
- Inbound data crosses back to the UI as **Qt signals delivered over a queued connection**.
  Because the endpoint and the UI objects live on different threads, Qt copies the signal
  argument into the receiver's event queue and runs the slot on the UI thread. No mutable
  state is shared across the boundary, so there is nothing to lock.

A queued signal and slot is the right tool at meter rate (tens of times per second). It is
simple, it is allocation-light enough for this rate, and it cannot deadlock. The threading
contract is strict and documented in the header: construct the endpoint, move it to the
worker thread, start the worker's event loop, and only ever invoke its slots through queued
calls, because a `QUdpSocket` may only be touched by the thread that created it.

At a much higher rate, namely the audio rate inside the real engine, this approach would no
longer fit, and the engine uses a different technique for that path (see below).

## The real-time audio engine

The C++ / RtAudio engine in [`audio-engine/`](audio-engine) plays real stems and applies
per-channel DSP with no added latency. Its design centre is the audio callback, which runs
on a high-priority real-time thread supplied by the audio driver.

The cardinal rule of that callback is that it must never block: no memory allocation, no
locks, no file I/O, nothing with unbounded or unpredictable timing. StageView follows that
rule strictly:

- Control parameters (gain, pan, mute, EQ and compressor settings) are stored as
  `std::atomic` values. The OSC server thread writes them; the audio thread reads them.
  There are no locks on the audio path.
- Meter values flow the other way through atomics as well: the audio thread publishes
  levels, and the OSC server thread reads and broadcasts them.
- The callback uses fixed, stack-allocated scratch buffers. It allocates nothing.

The DSP itself is chosen to add zero latency: minimum-phase biquad filters for a 3-band EQ
and a feed-forward (no-lookahead) compressor. The engine speaks the **same OSC protocol**
as the mock engine, so the surface drives real audio without any change.

### Sample-rate handling

Stems are often 44.1 kHz while an audio interface may run at 48 kHz. The engine opens the
device at the device's own preferred sample rate and resamples the stems to match, which
avoids the silent-output failure mode that a shared-mode rate mismatch otherwise causes on
Windows WASAPI. Device selection and an ASIO path are available through command-line flags.

## Output routing model

Outputs are organised as pairs, and each pair is independently configurable as either a
**stereo pair** or **two separate mono outputs**. This mirrors how a small digital mixer
exposes its outputs: a pair is never both stereo and dual-mono at the same time, it is one
or the other, and the choice is per pair rather than hardwired.

On top of that, each source carries a **send matrix**: a per-output send level that lets
every output (or stereo pair) receive its own monitor mix. The companion app picks an
output and adjusts the mix for that output, which is how a performer dials in a personal
monitor mix without touching anyone else's.

## The polyglot clients

The clients in [`clients/`](clients) exist to demonstrate that the protocol, not any single
language, is the integration point. Each client implements the same OSC wire format and
talks to the same engine. See [`clients/README.md`](clients/README.md) for the full list
and the rationale for each language. The short version: C++/Qt holds the real-time path,
and higher-level languages cover tooling, legacy control panels, and the touch companion.

## Repository layout

```
core/          OSC codec, threaded OscEndpoint, MIDI mapper (the stageview_core library)
app/           the Qt/QML surface: main, StageModel, MIDI input, QML scene
audio-engine/  the real-time C++ / RtAudio engine (stem playback + DSP + OSC server)
engine/        the Python mock engine, its OSC mirror, and Python tests
tests/         C++ tests: OSC codec, MIDI mapper, threaded endpoint (run via CTest)
clients/       polyglot companion clients (C#, Dart/Flutter, Object Pascal)
tools/         build, deploy, stem-prep and probe scripts
docs/          screenshot and supporting assets
.github/       CI workflow (3-OS build, CTest, Python tests)
```

## Build and test in one line each

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/<kit> && cmake --build build
ctest --test-dir build --output-on-failure
```

CI runs the same build and tests on Ubuntu, Windows and macOS on every push.
