# console-pascal - legacy console control (Object Pascal)

A native control application in **Object Pascal**, built with **Free Pascal** (the
open, Delphi-compatible compiler). It stands in for the kind of mature control
software that has driven digital consoles for years: plain, fast, native code, with
**scene recall** as its headline feature - pushing a whole stored set of channel
states to the engine in one action, the way an operator recalls a scene between songs.

**Why Object Pascal for this:** longevity. Delphi/Object Pascal was a popular way to
build serious, responsive Windows desktop apps quickly in the late 90s and 2000s, and
its strength is backward compatibility: that code still compiles and still runs decades
later. It stands in for the established, show-critical control application that there is
simply no reason to rewrite.

## Build & run

Requires Free Pascal (`fpc`). With the mock engine running
(`python engine/mock_engine.py`):

```pwsh
fpc stagectl.lpr          # produces stagectl(.exe)

./stagectl                # recall the built-in "Showtime" scene
./stagectl soundcheck     # recall a flat soundcheck scene
./stagectl silence        # mute everything
./stagectl set 6 1.0      # set channel 6 (Vox) level to 1.0
```

You can watch the effect land in the Qt surface (the pucks jump to the recalled
positions) or with `python tools/probe.py`.

## What it demonstrates

- Object Pascal: records, arrays, procedures, command-line handling, manual
  big-endian byte work - the OSC wire format implemented from scratch.
- Native UDP sockets via the Free Pascal `Sockets` unit (with WinSock startup on
  Windows).
- A real, useful control concept (scene recall) rather than a hello-world.
