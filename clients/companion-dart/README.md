# companion-dart - tablet companion / personal monitor mix (Dart)

The **Dart** core of a tablet companion app: personal monitor mixing and remote
control over OSC/UDP. Dart is **Flutter's language** - a Flutter touch UI would sit
directly on top of this (`CompanionClient.meters` is a `Stream` the widgets would
`StreamBuilder` over; `setMonitorLevel` is what a fader's `onChanged` would call). It
is kept deliberately UI-agnostic so the same logic backs a phone, tablet, desktop or
web build.

**Why Dart/Flutter is in a stack like this:** it's the newest layer, and the giveaway
is *cross-platform + touch*. One Dart/Flutter codebase runs on iOS, Android, desktop
and web with a slick, touch-first UI - exactly what you'd reach for to let a performer
adjust their own monitor mix from a phone, or to remote-control a console from an iPad.

> This is the **headless Dart core** - the OSC/monitor logic, runnable from the command
> line (handy for testing without a phone). The **Flutter touch app** that wraps it and
> **builds to an Android APK** now lives in [`../companion-flutter`](../companion-flutter).

## Build & run

Requires the Dart SDK (`dart`). With the mock engine running
(`python engine/mock_engine.py`):

```pwsh
dart run bin/companion.dart                  # watch your monitor meters for ~3s
dart run bin/companion.dart --boost 6 1.0    # push source 6 (Vox) to the top of your mix
```

## What it demonstrates

- Dart: classes, `async`/`Future`, broadcast `Stream`s, sealed-style `switch`
  expressions, `dart:typed_data` `ByteData` for big-endian - the OSC codec in Dart.
- A clean separation between transport/logic (this file) and UI (Flutter), which is
  exactly how a Flutter app is structured.
