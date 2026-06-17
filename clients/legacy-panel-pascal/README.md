# legacy-panel-pascal - legacy console-control GUI (Object Pascal / Lazarus)

A native Windows control panel in **Object Pascal**, built with **Lazarus/LCL** (the
free, Delphi-compatible RAD framework). This is what "legacy console control software"
actually looks like - the classic grey VCL form Delphi was used to build for two
decades: **scene-recall buttons**, **trackbar faders**, **mute checkboxes** and
**progress-bar meters**, all native Win32 controls.

It speaks the same OSC/UDP as every other StageView client, so it drives the real
engine: drag a fader and the channel level changes; hit **Showtime / Soundcheck /
Silence** and the whole mix recalls; meters update live.

> Why this and not just the console tool (`../console-pascal`)? Because Object Pascal's
> real reputation is *rapid native Windows GUIs*, not terminals. A control surface of
> that era was a form like this one - so this is the honest face of "the legacy Delphi
> control app."

## Build & run

Requires **Lazarus** (which bundles FPC + the LCL). With the engine running
(`stageaudio` or `python engine/mock_engine.py`):

```pwsh
lazbuild legacypanel.lpi       # produces legacypanel(.exe)
./legacypanel
```

The panel connects to `127.0.0.1:9000`, subscribes, and builds one row per channel
from the engine's snapshot.

## What it demonstrates

- **Object Pascal + LCL/Delphi-style GUI**: a `TForm` with controls created in code
  (`TButton`, `TTrackBar`, `TCheckBox`, `TProgressBar`), event handlers, a `TTimer`
  driving live updates - the classic RAD desktop idiom.
- **OSC/UDP from a legacy stack**: `oscnet.pas` implements the OSC wire format over
  WinSock (non-blocking receive, polled by the timer), the same protocol as the C++,
  C#, Dart and Python clients.
- **Scene recall** - pushing a stored mix to the engine in one action, the bread and
  butter of a console control app.

## Files

- `legacypanel.lpr` / `legacypanel.lpi` - program + Lazarus project.
- `mainform.pas` - the form (UI built in code, event handlers, OSC dispatch).
- `oscnet.pas` - OSC 1.0 codec + UDP socket (WinSock).
