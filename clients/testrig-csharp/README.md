# testrig-csharp - engine diagnostics / test rig (C#)

An automated diagnostics tool in **C# / .NET**. It connects to the engine over
OSC/UDP and runs a sequence of **PASS/FAIL checks** - the sort of jig a hardware team
runs against a unit on the production line, or in CI. It exits non-zero if any check
fails, so it drops straight into a pipeline.

**Why C# is in a stack like this:** it's the productivity-and-tooling language.
Memory-managed, huge standard library, superb on Windows via .NET, fast to write
utilities in. You wouldn't put it on the real-time audio path (that's C++'s job), but
it's a brilliant choice for everything *around* the core: test rigs, calibration and
provisioning tools, diagnostics, internal companion apps.

## Build & run

Requires the .NET SDK (`dotnet`). With the mock engine running
(`python engine/mock_engine.py`):

```pwsh
dotnet run                       # against 127.0.0.1:9000
dotnet run -- 127.0.0.1 9000     # explicit host/port
```

Example output:

```
Diagnostics:
  [PASS] engine reachable / reports channels  (8 channels)
  [PASS] all channels named  (Kick, Snare, Hat, Bass, Gtr, Keys, Vox, FX)
  [PASS] meter feed is streaming  (36 frames/1.2s)
  [PASS] meter follows level (ch0 loud > quiet)  (hi=0.74 lo=0.03)
  [PASS] mute silences channel (ch1 ~ 0)  (meter=0.01)

5 passed, 0 failed.
```

## What it demonstrates

- C#: `async`/`await` UDP I/O, `record`/tuple returns, pattern matching, LINQ,
  spans + `BinaryPrimitives` for big-endian - the OSC codec implemented in C#.
- A genuinely useful tool: automated, closed-loop verification of the engine
  (drive a parameter, observe the meter, assert the behaviour) with a process exit
  code suitable for CI.
