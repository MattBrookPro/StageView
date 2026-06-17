# dashboard-csharp - diagnostics & control dashboard (C# / WinForms)

A native-Windows GUI in **C# / WinForms** - the kind of internal tooling a hardware
team actually builds in C#, where its desktop-GUI strength pays off. It connects to the
StageView engine over OSC/UDP and gives you per-channel **faders / pan / mute / meters**,
**scene recall** (Showtime / Soundcheck / Silence), and a **Run Diagnostics** button that
runs the same automated checks as the console test rig - behind a GUI.

This is the C# counterpart to the Object Pascal `legacy-panel-pascal`: both are native
Windows control/diagnostics tools, one classic-Delphi, one classic-.NET. The console
rig in [`../testrig-csharp`](../testrig-csharp) stays as the headless/CI variant.

## Build & run

Requires the .NET SDK. With the engine running (`stageaudio` or `python engine/mock_engine.py`):

```pwsh
dotnet run -c Release        # or: dotnet build, then run bin/Release/net8.0-windows/stagedashboard.exe
```

The dashboard subscribes, builds a row per channel from the engine snapshot, and
streams meters live. **Run Diagnostics** drives `ch0`'s level and asserts its meter
follows - results logged with PASS/FAIL in the panel at the bottom.

## What it demonstrates

- **C# / WinForms**: a real desktop UI built in code (`TrackBar`, `CheckBox`,
  `ProgressBar`, `Button`, a `Timer`), event handling, `async`/`await` for the
  non-blocking diagnostics, all on the UI thread without freezing it.
- The same hand-written **OSC codec** (`Osc.cs`) as the console rig and every other
  client - the GUI is just a different front end on the same protocol.
- Authentic role: control + visual diagnostics, exactly where C# tooling fits around a
  real-time core.

## Files

- `Dashboard.csproj` - WinForms project (`net8.0-windows`, `UseWindowsForms`).
- `Program.cs` - entry point.
- `MainForm.cs` - the form (UI in code, OSC dispatch, scene recall, diagnostics).
- `Osc.cs` - OSC 1.0 codec.
