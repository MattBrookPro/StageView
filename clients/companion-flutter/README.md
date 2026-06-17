# companion-flutter - tablet companion app (Dart / Flutter, Android APK)

The real **Flutter** companion: a touch app for **personal monitor mixing** that
controls the StageView engine over WiFi. It's the deployable version of the Dart logic
in [`../companion-dart`](../companion-dart) - same OSC protocol, now with a Flutter UI
that builds to an **Android APK**.

Each source gets a **fader** (your monitor send level) and a **live meter**; the engine
runs on the PC, the phone is just another OSC client on the LAN. This is the layer
that lets a performer mix their own monitors from a phone.

## Build the APK

Requires the Flutter SDK + Android SDK + a JDK 17.

```bash
flutter pub get
flutter build apk --release      # -> build/app/outputs/flutter-apk/app-release.apk
```

Install the APK on a phone on the **same WiFi** as the PC, open it, set **Engine IP** to
the PC's LAN address, and tap **Connect** with the audio engine (`stageaudio`) running.
(The engine binds `0.0.0.0:9000`, so it accepts LAN clients and replies to each.)

You can also run it on desktop for a quick check: `flutter run -d windows`.

## What it demonstrates

- **Flutter / Dart**: a real Material touch UI (faders, live meters, connection state),
  `ChangeNotifier` state, `ListenableBuilder`, dark theming.
- The same hand-written **OSC codec** (`lib/osc.dart`) as every other client - the phone
  speaks the identical wire format to the same engine.
- Clean separation: `lib/companion_client.dart` is the UDP/OSC + monitor logic (no UI),
  `lib/main.dart` is the Flutter UI on top - exactly how a Flutter app is structured.

## Files

- `lib/osc.dart` - OSC 1.0 codec (Dart).
- `lib/companion_client.dart` - `CompanionClient` (UDP socket, subscribe, meters, setLevel).
- `lib/main.dart` - the touch UI.
- `android/app/src/main/AndroidManifest.xml` - adds the `INTERNET` permission for UDP.
