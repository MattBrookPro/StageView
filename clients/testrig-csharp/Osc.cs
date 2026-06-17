using System.Buffers.Binary;
using System.Text;

namespace StageRig;

/// <summary>
/// Minimal OSC 1.0 codec - the C# sibling of the C++ (core/Osc) and Python
/// (engine/osc.py) implementations. Same wire format (address + type-tag string +
/// 4-byte-aligned big-endian args), so this tool talks to the very same engine.
/// Supports int32 'i', float32 'f', string 's'.
/// </summary>
public static class Osc
{
    public static byte[] Encode(string address, params object[] args)
    {
        using var ms = new MemoryStream();
        WriteString(ms, address);

        var tags = new StringBuilder(",");
        foreach (var a in args)
            tags.Append(a switch
            {
                int => 'i',
                float => 'f',
                double => 'f',
                string => 's',
                _ => throw new ArgumentException($"unsupported OSC arg type: {a.GetType()}")
            });
        WriteString(ms, tags.ToString());

        foreach (var a in args)
        {
            switch (a)
            {
                case int i: WriteInt32(ms, i); break;
                case float f: WriteFloat32(ms, f); break;
                case double d: WriteFloat32(ms, (float)d); break;
                case string s: WriteString(ms, s); break;
            }
        }
        return ms.ToArray();
    }

    public static (string Address, List<object> Args) Decode(byte[] data)
    {
        int pos = 0;
        string address = ReadString(data, ref pos);
        if (!address.StartsWith('/'))
            throw new FormatException("OSC address must start with '/'");

        string tags = ReadString(data, ref pos);
        if (!tags.StartsWith(','))
            throw new FormatException("OSC type-tag string must start with ','");

        var args = new List<object>();
        foreach (char t in tags[1..])
        {
            switch (t)
            {
                case 'i':
                    args.Add(BinaryPrimitives.ReadInt32BigEndian(data.AsSpan(pos, 4)));
                    pos += 4;
                    break;
                case 'f':
                    args.Add(BinaryPrimitives.ReadSingleBigEndian(data.AsSpan(pos, 4)));
                    pos += 4;
                    break;
                case 's':
                    args.Add(ReadString(data, ref pos));
                    break;
                default:
                    throw new FormatException($"unsupported OSC type tag '{t}'");
            }
        }
        return (address, args);
    }

    private static void WriteString(Stream s, string str)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(str);
        s.Write(bytes);
        s.WriteByte(0); // terminating null
        int field = bytes.Length + 1;
        for (int pad = (4 - field % 4) % 4; pad > 0; pad--)
            s.WriteByte(0);
    }

    private static void WriteInt32(Stream s, int v)
    {
        Span<byte> b = stackalloc byte[4];
        BinaryPrimitives.WriteInt32BigEndian(b, v);
        s.Write(b);
    }

    private static void WriteFloat32(Stream s, float v)
    {
        Span<byte> b = stackalloc byte[4];
        BinaryPrimitives.WriteSingleBigEndian(b, v);
        s.Write(b);
    }

    private static string ReadString(byte[] data, ref int pos)
    {
        int start = pos;
        while (pos < data.Length && data[pos] != 0)
            pos++;
        string s = Encoding.UTF8.GetString(data, start, pos - start);
        int field = (pos - start) + 1; // include the null
        pos = start + field + (4 - field % 4) % 4;
        return s;
    }
}
