using System.Net.Sockets;
using StageRig;

// StageView engine diagnostics / production test rig (C#).
//
// Connects to the audio engine over OSC/UDP and runs a sequence of automated
// PASS/FAIL checks - the sort of jig a hardware team runs against a unit on the
// production line, or in CI. It is deliberately *not* on the real-time path: that
// stays C++. This is the productivity-tooling layer around it, which is exactly
// where C# earns its place in a stack like this.
//
// Exits 0 if every check passes, 1 otherwise - so it composes into a build/test
// pipeline. Run the mock engine first:  python engine/mock_engine.py

string host = args.Length > 0 ? args[0] : "127.0.0.1";
int port = args.Length > 1 ? int.Parse(args[1]) : 9000;

Console.WriteLine($"StageView test rig -> udp://{host}:{port}\n");

using var udp = new UdpClient();
udp.Connect(host, port); // fixes the remote end and filters replies to the engine

int pass = 0, fail = 0;
void Check(string name, bool ok, string? detail = null)
{
    Console.WriteLine($"  [{(ok ? "PASS" : "FAIL")}] {name}{(detail is null ? "" : $"  ({detail})")}");
    if (ok) pass++; else fail++;
}

void Send(string address, params object[] a) => udp.Send(Osc.Encode(address, a));

// Receive the next OSC message, or null if `deadline` passes first.
async Task<(string Address, List<object> Args)?> Recv(DateTime deadline)
{
    TimeSpan remaining = deadline - DateTime.UtcNow;
    if (remaining <= TimeSpan.Zero) return null;
    using var cts = new CancellationTokenSource(remaining);
    try
    {
        UdpReceiveResult r = await udp.ReceiveAsync(cts.Token);
        return Osc.Decode(r.Buffer);
    }
    catch (OperationCanceledException) { return null; }
    catch (FormatException) { return ("", new List<object>()); } // skip garbage, keep going
}

// Drain meter frames for `ms` and return the average meter value of one channel.
async Task<double> AverageMeter(int channel, int ms)
{
    DateTime end = DateTime.UtcNow.AddMilliseconds(ms);
    double sum = 0; int n = 0;
    while (await Recv(end) is { } msg)
    {
        if (msg.Address == "/meters" && channel < msg.Args.Count)
        {
            sum += (float)msg.Args[channel];
            n++;
        }
    }
    return n > 0 ? sum / n : double.NaN;
}

// --- Subscribe and gather the snapshot + a window of meters ---
Send("/subscribe");

int channelCount = 0;
var names = new Dictionary<int, string>();
int meterFrames = 0;
DateTime gatherEnd = DateTime.UtcNow.AddMilliseconds(1200);
while (await Recv(gatherEnd) is { } m)
{
    switch (m.Address)
    {
        case "/stage/channels":
            channelCount = (int)m.Args[0];
            break;
        case "/meters":
            meterFrames++;
            break;
        default:
            if (m.Address.StartsWith("/channel/"))
            {
                string[] p = m.Address.Split('/'); // "", "channel", "<n>", "<param>"
                if (p.Length == 4 && p[3] == "name")
                    names[int.Parse(p[2])] = (string)m.Args[0];
            }
            break;
    }
}

Console.WriteLine("Diagnostics:");
Check("engine reachable / reports channels", channelCount > 0, $"{channelCount} channels");
Check("all channels named", names.Count == channelCount && names.Values.All(s => !string.IsNullOrEmpty(s)),
      string.Join(", ", names.OrderBy(kv => kv.Key).Select(kv => kv.Value)));
Check("meter feed is streaming", meterFrames >= 5, $"{meterFrames} frames/1.2s");

// --- Meter must follow level: drive ch0 hot, then cold ---
Send("/channel/0/level", 1.0f);
double hi = await AverageMeter(0, 700);
Send("/channel/0/level", 0.0f);
double lo = await AverageMeter(0, 900);
Check("meter follows level (ch0 loud > quiet)", hi > lo + 0.1, $"hi={hi:F2} lo={lo:F2}");

// --- Mute must silence a channel ---
Send("/channel/1/level", 1.0f);
Send("/channel/1/mute", 1);
double muted = await AverageMeter(1, 900);
Send("/channel/1/mute", 0); // restore
// NaN (no frames seen) fails the comparison, which is the right outcome.
Check("mute silences channel (ch1 ~ 0)", muted < 0.05, $"meter={muted:F2}");

Send("/unsubscribe");

Console.WriteLine($"\n{pass} passed, {fail} failed.");
return fail == 0 ? 0 : 1;
