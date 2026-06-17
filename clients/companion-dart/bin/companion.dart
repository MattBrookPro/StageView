// StageView tablet companion - personal monitor mixing (Dart).
//
// The newest layer in a pro-audio stack tends to be a cross-platform, touch
// companion app: a performer adjusting their own monitor mix from a phone or tablet.
// That's Flutter territory, and Flutter's language is Dart. This file is the Dart
// core of that companion - the OSC/UDP client and monitor-mix logic - written so a
// Flutter UI binds straight onto it (CompanionClient.meters is a Stream the widgets
// would listen to; setMonitorLevel is what a fader's onChanged would call).
//
// Run the engine first (python engine/mock_engine.py), then:
//   dart run bin/companion.dart                 # watch your monitor meters for ~3s
//   dart run bin/companion.dart --boost 6 1.0   # push source 6 (Vox) to the top

import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

const engineHost = '127.0.0.1';
const enginePort = 9000;

/// Minimal OSC 1.0 codec - the Dart sibling of the C++/Python/C# ones.
class Osc {
  static Uint8List encode(String address, [List<Object> args = const []]) {
    final out = BytesBuilder();
    _writeString(out, address);

    final tags = StringBuffer(',');
    for (final a in args) {
      tags.write(switch (a) {
        int _ => 'i',
        double _ => 'f',
        String _ => 's',
        _ => throw ArgumentError('unsupported OSC arg: $a'),
      });
    }
    _writeString(out, tags.toString());

    for (final a in args) {
      switch (a) {
        case int v:
          out.add((ByteData(4)..setInt32(0, v, Endian.big)).buffer.asUint8List());
        case double v:
          out.add((ByteData(4)..setFloat32(0, v, Endian.big)).buffer.asUint8List());
        case String v:
          _writeString(out, v);
      }
    }
    return out.toBytes();
  }

  static (String address, List<Object> args) decode(Uint8List data) {
    var pos = 0;
    final (address, p1) = _readString(data, pos);
    pos = p1;
    if (!address.startsWith('/')) throw FormatException('OSC address must start with /');
    final (tags, p2) = _readString(data, pos);
    pos = p2;
    if (!tags.startsWith(',')) throw FormatException('OSC tags must start with ,');

    final bd = ByteData.sublistView(data);
    final args = <Object>[];
    for (final t in tags.substring(1).split('')) {
      switch (t) {
        case 'i':
          args.add(bd.getInt32(pos, Endian.big));
          pos += 4;
        case 'f':
          args.add(bd.getFloat32(pos, Endian.big));
          pos += 4;
        case 's':
          final (s, p) = _readString(data, pos);
          args.add(s);
          pos = p;
        default:
          throw FormatException('unsupported OSC tag: $t');
      }
    }
    return (address, args);
  }

  static void _writeString(BytesBuilder out, String s) {
    out.add(s.codeUnits);
    out.addByte(0); // terminator
    final field = s.length + 1;
    out.add(List.filled((4 - field % 4) % 4, 0)); // pad to 4
  }

  static (String, int) _readString(Uint8List data, int pos) {
    var end = pos;
    while (end < data.length && data[end] != 0) end++;
    final s = String.fromCharCodes(data.sublist(pos, end));
    final field = (end - pos) + 1;
    return (s, pos + field + (4 - field % 4) % 4);
  }
}

/// The companion's logic layer - UI-agnostic, so a Flutter app drives it directly.
class CompanionClient {
  RawDatagramSocket? _socket;
  final _meters = StreamController<List<double>>.broadcast();
  final names = <int, String>{};
  int channelCount = 0;

  /// Live monitor meters - a Flutter widget would `StreamBuilder` over this.
  Stream<List<double>> get meters => _meters.stream;

  Future<void> connect() async {
    final socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
    _socket = socket;
    final engine = InternetAddress(engineHost);

    socket.listen((event) {
      if (event != RawSocketEvent.read) return;
      final dg = socket.receive();
      if (dg == null) return;
      try {
        final (address, args) = Osc.decode(dg.data);
        _dispatch(address, args);
      } on FormatException {
        // ignore malformed datagrams
      }
    });

    socket.send(Osc.encode('/subscribe'), engine, enginePort);
  }

  /// What a monitor fader's onChanged would call: set your send level for a source.
  void setMonitorLevel(int source, double level) =>
      _send('/channel/$source/level', [level.clamp(0.0, 1.0)]);

  void _send(String address, List<Object> args) {
    _socket?.send(Osc.encode(address, args), InternetAddress(engineHost), enginePort);
  }

  void _dispatch(String address, List<Object> args) {
    if (address == '/meters') {
      _meters.add(args.map((a) => (a as double)).toList());
    } else if (address == '/stage/channels') {
      channelCount = args.first as int;
    } else if (address.startsWith('/channel/')) {
      final p = address.split('/'); // '', 'channel', '<n>', '<param>'
      if (p.length == 4 && p[3] == 'name') names[int.parse(p[2])] = args.first as String;
    }
  }

  void close() {
    _socket?.close();
    _meters.close();
  }
}

Future<void> main(List<String> argv) async {
  final client = CompanionClient();
  await client.connect();
  print('StageView companion -> udp://$engineHost:$enginePort');

  // Optional: --boost <channel> <level> demonstrates a monitor-send change.
  final boostIdx = argv.indexOf('--boost');
  if (boostIdx >= 0 && boostIdx + 2 < argv.length) {
    final ch = int.parse(argv[boostIdx + 1]);
    final lvl = double.parse(argv[boostIdx + 2]);
    // small delay so the snapshot (names) arrives first for a nicer print
    await Future.delayed(const Duration(milliseconds: 300));
    client.setMonitorLevel(ch, lvl);
    print('Set my monitor send for "${client.names[ch] ?? 'ch$ch'}" to ${lvl.toStringAsFixed(2)}');
  }

  // Stream a few seconds of monitor meters - the "what you'd see on the tablet" view.
  var frames = 0;
  final sub = client.meters.listen((levels) {
    frames++;
    final bars = [
      for (var i = 0; i < levels.length; i++)
        '${client.names[i] ?? 'ch$i'}:${levels[i].toStringAsFixed(2)}'
    ].join('  ');
    print(bars);
  });

  await Future.delayed(const Duration(seconds: 3));
  await sub.cancel();
  client.close();
  print('\n($frames monitor frames received)');
}
