# StageView companion clients (the polyglot ecosystem)

StageView's surface and engine talk over **OSC on UDP**, and that protocol is
**language-neutral**. These companion clients demonstrate it: each is written in a
different language, each implements the same small OSC wire format, and each talks to the
*same* engine (`python engine/mock_engine.py`) alongside the Qt/QML surface.

It also mirrors how these languages tend to coexist around a real pro-audio product, and
it is a practical way to learn a polyglot codebase: learn the protocol, then speak it from
whatever layer you are working in.

| Client | Language | Role here | Where it fits |
|---|---|---|---|
| [`legacy-panel-pascal/`](legacy-panel-pascal) | **Object Pascal** (Lazarus/LCL) | **Legacy control-panel GUI**: scene buttons, trackbar faders, mute, meters | The mature, native-Windows control software (classic grey VCL form) that Delphi was a natural fit for: runs shows for years and still compiles. |
| [`console-pascal/`](console-pascal) | **Object Pascal** (Free Pascal) | The same control, as a console scene-recall CLI | Headless variant, handy for scripting and testing without the GUI. |
| [`dashboard-csharp/`](dashboard-csharp) | **C# / WinForms** | **Diagnostics and control dashboard** (faders, meters, scenes, run-checks) | Internal Windows tooling, where C#'s desktop-GUI strength pays off around the real-time core. |
| [`testrig-csharp/`](testrig-csharp) | **C# / .NET** | The same checks as a console **test rig** (PASS/FAIL, CI exit code) | Headless variant: test jigs and CI, no GUI. |
| [`companion-flutter/`](companion-flutter) | **Dart / Flutter** | **Tablet companion app**, personal monitor mixing, builds to an **Android APK** | The newest, cross-platform, touch-first layer: a phone or tablet app to mix your own monitors. |
| [`companion-dart/`](companion-dart) | **Dart** | The same companion as a headless CLI (logic only) | The UI-free core the Flutter app is built on, handy for testing without a device. |

The real-time path itself stays **C++/Qt** (the surface), which is the layering you would
expect: bare-metal-capable C++ for anything time-critical, and faster-to-write,
higher-level languages for the tooling, legacy control, and touch companions around it.

### Why each language, in one line

- **Object Pascal**: native, fast, and known for *longevity*. Decades-old control code
  still compiles and runs, so there is no reason to rewrite it.
- **C#**: extremely productive and managed; ideal for the test rigs, config tools and
  diagnostics that surround a hardware product, where you do not need C++'s real-time
  control.
- **Dart/Flutter**: one codebase to iOS/Android/desktop with a touch-first UI; a natural
  choice for a companion tablet or phone app.

Each client folder has its own README with build and run steps. They all assume the mock
engine is running on `udp://127.0.0.1:9000`.
