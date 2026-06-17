import 'dart:typed_data';

/// Minimal OSC 1.0 codec - the same wire format as the C++/Python/C# sides, so
/// this Flutter app talks to the very same engine. int32 'i', float32 'f',
/// string 's'.
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
    if (!address.startsWith('/')) throw const FormatException('bad OSC address');
    final (tags, p2) = _readString(data, pos);
    pos = p2;
    if (!tags.startsWith(',')) throw const FormatException('bad OSC tags');

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
    out.addByte(0);
    final field = s.length + 1;
    out.add(List.filled((4 - field % 4) % 4, 0));
  }

  static (String, int) _readString(Uint8List data, int pos) {
    var end = pos;
    while (end < data.length && data[end] != 0) {
      end++;
    }
    final s = String.fromCharCodes(data.sublist(pos, end));
    final field = (end - pos) + 1;
    return (s, pos + field + (4 - field % 4) % 4);
  }
}
