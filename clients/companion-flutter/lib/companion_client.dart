import 'dart:io';

import 'package:flutter/foundation.dart';

import 'osc.dart';

/// One source's state as the companion sees it.
class Channel {
  final int index;
  String name;
  double level; // stereo-main fader (outputs 1-2)
  double meter; // live level from the engine
  final Map<int, double> sends = {}; // per-output send level (output index -> 0..1)

  Channel(this.index, {this.name = '', this.level = 0.8, this.meter = 0.0});
}

/// The companion's logic layer: a UDP/OSC client to the StageView engine. It tracks
/// the device's hardware outputs and a "destination" you're mixing - either the
/// stereo main (bus -1) or a single output (bus k) - and the faders write to that
/// destination, so each output is an independent monitor mix.
class CompanionClient extends ChangeNotifier {
  CompanionClient({this.port = 9000});

  final int port;
  String host = '';
  bool connected = false;
  final List<Channel> channels = [];
  int outputs = 2; // hardware outputs the engine reported
  final Map<int, bool> pairSplit = {}; // output-pair index -> split (false = stereo)
  // Current destination: a mono output (>=0) or a stereo pair encoded as -(pair+1).
  int bus = -1;

  RawDatagramSocket? _socket;

  Future<void> connect(String newHost) async {
    host = newHost;
    _socket?.close();
    channels.clear();
    outputs = 2;
    pairSplit.clear();
    bus = -1;
    connected = false;
    notifyListeners();

    final socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
    _socket = socket;
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

    _send('/subscribe');
    connected = true;
    notifyListeners();
  }

  // --- destinations (buses) ---
  // Each output pair is independently a stereo bus or two mono outs.
  // A mono output is encoded as its index (>=0); a stereo pair p as -(p+1).
  int get numPairs => outputs ~/ 2;
  int pairOf(int b) => b >= 0 ? b ~/ 2 : -b - 1;
  bool isPairStereo(int p) => !(pairSplit[p] ?? false);

  List<int> get buses {
    final list = <int>[];
    for (var p = 0; p < numPairs; p++) {
      if (isPairStereo(p)) {
        list.add(-(p + 1)); // one stereo destination
      } else {
        list..add(2 * p)..add(2 * p + 1); // two mono outs
      }
    }
    if (outputs.isOdd) list.add(outputs - 1); // trailing mono output
    return list;
  }

  String busLabel(int b) {
    if (b >= 0) return 'Out ${b + 1}';
    final p = -b - 1;
    return 'Out ${2 * p + 1}-${2 * p + 2}';
  }

  bool get selectedPairStereo => isPairStereo(pairOf(bus));

  void selectBus(int b) {
    bus = b;
    notifyListeners();
  }

  /// Toggle whether the currently selected destination's pair is stereo or split.
  void setSelectedPairStereo(bool stereo) {
    final p = pairOf(bus);
    pairSplit[p] = !stereo;
    _send('/device/pair/$p/split', [stereo ? 0 : 1]);
    bus = stereo ? -(p + 1) : 2 * p; // keep a valid destination for this pair
    notifyListeners();
  }

  /// The fader value for a source on the currently selected destination.
  double valueFor(Channel c) {
    if (bus >= 0) return c.sends[bus] ?? 0.0;
    final p = -bus - 1;
    if (p == 0) return c.level; // pair 0 stereo = the spatial main (pan preserved)
    return c.sends[2 * p] ?? 0.0; // other stereo pairs: centered send (left = right)
  }

  /// Move a source's fader on the selected destination.
  void setValue(int channelIdx, double v) {
    if (channelIdx < 0 || channelIdx >= channels.length) return;
    final c = channels[channelIdx];
    if (bus >= 0) {
      c.sends[bus] = v;
      _send('/channel/$channelIdx/out/$bus', [v]); // mono output send
      notifyListeners();
      return;
    }
    final p = -bus - 1;
    if (p == 0) {
      c.level = v; // pair 0 stereo is the desktop main; keep its pan
      _send('/channel/$channelIdx/level', [v]);
    } else {
      c.sends[2 * p] = v; // other stereo pair: send equally to both outs
      c.sends[2 * p + 1] = v;
      _send('/channel/$channelIdx/out/${2 * p}', [v]);
      _send('/channel/$channelIdx/out/${2 * p + 1}', [v]);
    }
    notifyListeners();
  }

  void _send(String address, [List<Object> args = const []]) {
    final s = _socket;
    if (s != null && host.isNotEmpty) {
      s.send(Osc.encode(address, args), InternetAddress(host), port);
    }
  }

  Channel _ensure(int i) {
    while (channels.length <= i) {
      channels.add(Channel(channels.length));
    }
    return channels[i];
  }

  void _dispatch(String address, List<Object> args) {
    if (address == '/meters') {
      for (var i = 0; i < args.length && i < channels.length; i++) {
        channels[i].meter = (args[i] as double);
      }
      notifyListeners();
    } else if (address == '/device/outputs') {
      outputs = args.first as int;
      notifyListeners();
    } else if (address.startsWith('/device/pair/')) {
      final parts = address.split('/'); // '', device, pair, <p>, split
      if (parts.length == 5 && parts[4] == 'split' && args.isNotEmpty) {
        final p = int.tryParse(parts[3]);
        if (p != null) {
          pairSplit[p] = (args.first as int) != 0;
          if (!buses.contains(bus)) bus = buses.isNotEmpty ? buses.first : -1;
        }
      }
      notifyListeners();
    } else if (address == '/stage/channels') {
      _ensure((args.first as int) - 1);
      notifyListeners();
    } else if (address.startsWith('/channel/')) {
      final parts = address.split('/'); // '', 'channel', '<n>', 'param', ...
      if (parts.length >= 4 && args.isNotEmpty) {
        final c = _ensure(int.parse(parts[2]));
        final param = parts.sublist(3).join('/');
        if (param == 'name') {
          c.name = args.first as String;
        } else if (param == 'level') {
          c.level = (args.first as num).toDouble();
        } else if (param.startsWith('out/')) {
          final k = int.tryParse(param.substring(4));
          if (k != null) c.sends[k] = (args.first as num).toDouble();
        }
        notifyListeners();
      }
    }
  }

  @override
  void dispose() {
    _socket?.close();
    super.dispose();
  }
}
