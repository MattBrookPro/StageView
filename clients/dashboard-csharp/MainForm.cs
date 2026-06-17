using System.Net;
using System.Net.Sockets;

namespace StageDashboard;

/// <summary>
/// StageView diagnostics &amp; control dashboard (C# / WinForms). Connects to the
/// engine over OSC/UDP, shows per-channel faders / pan / mute / meters and scene
/// recall, and runs the same automated checks as the console test rig - behind a GUI.
/// </summary>
public sealed class MainForm : Form
{
    // Showtime balance (matches the stems: Vocal, Drums, Bass, Gtr L/R/Arr, Synth, Perc).
    private static readonly float[] Showtime = { 0.90f, 0.85f, 0.82f, 0.66f, 0.66f, 0.60f, 0.62f, 0.58f };

    private sealed record Row(Label Name, TrackBar Level, TrackBar Pan, CheckBox Mute, ProgressBar Meter);

    private readonly UdpClient _udp = new();
    private readonly System.Windows.Forms.Timer _timer = new();
    private readonly Label _status = new();
    private readonly TextBox _log = new();
    private readonly Panel _rows = new();
    private readonly Button _diagBtn;

    private readonly List<Row> _row = new();
    private int _count;
    private bool _connected;
    private float[] _meters = Array.Empty<float>();
    private int _meterFrames;

    public MainForm()
    {
        Text = "StageView - Diagnostics & Control";
        ClientSize = new Size(770, 540);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;

        Controls.Add(new Label
        {
            Text = "StageView  -  Diagnostics & Control",
            Font = new Font("Segoe UI", 12F, FontStyle.Bold),
            Location = new Point(12, 10),
            AutoSize = true
        });
        _status.Text = "offline";
        _status.ForeColor = Color.Gray;
        _status.Location = new Point(650, 16);
        _status.AutoSize = true;
        Controls.Add(_status);

        Controls.Add(MakeButton("Showtime", 12, 88, (_, _) => RecallScene("Showtime", Showtime, false)));
        Controls.Add(MakeButton("Soundcheck", 104, 88, (_, _) => RecallScene("Soundcheck", new[] { 0.7f }, false)));
        Controls.Add(MakeButton("Silence", 196, 88, (_, _) => RecallScene("Silence", new[] { 0f }, true)));
        _diagBtn = MakeButton("Run Diagnostics", 612, 146, async (_, _) => await RunDiagnostics());
        Controls.Add(_diagBtn);

        AddHeader("Channel", 14);
        AddHeader("Level", 92);
        AddHeader("Pan (L - R)", 282);
        AddHeader("Mute", 414);
        AddHeader("Meter", 474);

        _rows.SetBounds(8, 90, 754, 320);
        _rows.AutoScroll = true;
        Controls.Add(_rows);

        _log.Multiline = true;
        _log.ReadOnly = true;
        _log.ScrollBars = ScrollBars.Vertical;
        _log.Font = new Font("Consolas", 9F);
        _log.SetBounds(8, 418, 754, 112);
        Controls.Add(_log);

        _udp.Connect(IPAddress.Loopback, 9000);
        Send("/subscribe");

        _timer.Interval = 50;
        _timer.Tick += OnTick;
        _timer.Start();
    }

    private Button MakeButton(string text, int x, int w, EventHandler onClick)
    {
        var b = new Button { Text = text, Location = new Point(x, 38), Size = new Size(w, 26) };
        b.Click += onClick;
        return b;
    }

    private void AddHeader(string text, int x)
        => Controls.Add(new Label { Text = text, Location = new Point(x, 72), AutoSize = true, ForeColor = Color.DimGray });

    private void Send(string address, params object[] args) => _udp.Send(Osc.Encode(address, args));

    private void BuildRows(int n)
    {
        _count = n;
        for (int i = 0; i < n; i++)
        {
            int y = i * 34;
            var name = new Label { Text = $"Ch {i + 1}", Location = new Point(4, y + 6), Size = new Size(72, 16) };

            var level = new TrackBar { Minimum = 0, Maximum = 100, TickFrequency = 25, Tag = i };
            level.SetBounds(78, y, 180, 30);
            level.Scroll += (s, _) => Send($"/channel/{(int)((TrackBar)s!).Tag!}/level", ((TrackBar)s).Value / 100f);

            var pan = new TrackBar { Minimum = -100, Maximum = 100, Value = 0, TickFrequency = 100, Tag = i };
            pan.SetBounds(270, y, 130, 30);
            pan.Scroll += (s, _) => Send($"/channel/{(int)((TrackBar)s!).Tag!}/pan", ((TrackBar)s).Value / 100f);

            var mute = new CheckBox { Text = "Mute", Location = new Point(410, y + 6), Size = new Size(56, 20), Tag = i };
            mute.Click += (s, _) => Send($"/channel/{(int)((CheckBox)s!).Tag!}/mute", ((CheckBox)s).Checked ? 1 : 0);

            var meter = new ProgressBar { Minimum = 0, Maximum = 100, Style = ProgressBarStyle.Continuous };
            meter.SetBounds(470, y + 8, 268, 14);

            _rows.Controls.AddRange(new Control[] { name, level, pan, mute, meter });
            _row.Add(new Row(name, level, pan, mute, meter));
        }
    }

    private void RecallScene(string name, float[] levels, bool muted)
    {
        if (_count == 0) return;
        for (int i = 0; i < _count; i++)
        {
            float lvl = levels.Length > 0 ? levels[i % levels.Length] : 0f;
            Send($"/channel/{i}/level", lvl);
            Send($"/channel/{i}/mute", muted ? 1 : 0);
            // Programmatic updates don't fire Scroll/Click, so no echo back to the engine.
            _row[i].Level.Value = (int)Math.Round(lvl * 100);
            _row[i].Mute.Checked = muted;
        }
        _status.Text = "scene: " + name;
    }

    private void OnTick(object? sender, EventArgs e)
    {
        while (_udp.Available > 0)
        {
            IPEndPoint? ep = null;
            byte[] data;
            try { data = _udp.Receive(ref ep); } catch { break; }

            (string addr, List<object> args) m;
            try { m = Osc.Decode(data); } catch { continue; }

            if (!_connected) { _connected = true; _status.Text = "connected"; _status.ForeColor = Color.SeaGreen; }
            Dispatch(m.addr, m.args);
        }
    }

    private void Dispatch(string addr, List<object> args)
    {
        if (addr == "/stage/channels")
        {
            if (_count == 0 && args.Count > 0) BuildRows((int)args[0]);
        }
        else if (addr == "/meters")
        {
            _meterFrames++;
            _meters = args.Select(a => (float)a).ToArray();
            for (int i = 0; i < _meters.Length && i < _row.Count; i++)
                _row[i].Meter.Value = Math.Clamp((int)(_meters[i] * 100), 0, 100);
        }
        else if (addr.StartsWith("/channel/"))
        {
            string[] p = addr.Split('/'); // "", "channel", "<n>", "param", ...
            if (p.Length >= 4 && args.Count > 0 && int.TryParse(p[2], out int i) && i < _row.Count)
            {
                switch (p[3])
                {
                    case "name": _row[i].Name.Text = (string)args[0]; break;
                    case "level": _row[i].Level.Value = Math.Clamp((int)((float)args[0] * 100), 0, 100); break;
                    case "pan": _row[i].Pan.Value = Math.Clamp((int)((float)args[0] * 100), -100, 100); break;
                    case "mute": _row[i].Mute.Checked = (int)args[0] != 0; break;
                }
            }
        }
    }

    private void Log(string line) => _log.AppendText(line + Environment.NewLine);

    private void Check(string name, bool ok, string detail)
        => Log($"[{(ok ? "PASS" : "FAIL")}] {name}  ({detail})");

    // Same automated checks as the console rig, run from the GUI without blocking it.
    private async Task RunDiagnostics()
    {
        _diagBtn.Enabled = false;
        try
        {
            Log("--- diagnostics ---");
            Check("engine reachable / reports channels", _count > 0, $"{_count} channels");
            Check("meter feed is streaming", _meterFrames > 5, $"{_meterFrames} frames");

            Send("/channel/0/level", 1.0f);
            await Task.Delay(700);
            float hi = _meters.Length > 0 ? _meters[0] : 0f;
            Send("/channel/0/level", 0.0f);
            await Task.Delay(900);
            float lo = _meters.Length > 0 ? _meters[0] : 0f;
            Check("meter follows level (ch0 loud > quiet)", hi > lo + 0.1f, $"hi={hi:F2} lo={lo:F2}");

            Log("done. (recall a scene to restore levels)");
        }
        finally
        {
            _diagBtn.Enabled = true;
        }
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        _timer.Stop();
        try { Send("/unsubscribe"); } catch { }
        _udp.Dispose();
        base.OnFormClosed(e);
    }
}
