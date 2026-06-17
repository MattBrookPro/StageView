import 'package:flutter/material.dart';

import 'companion_client.dart';

void main() => runApp(const CompanionApp());

const _accent = Color(0xFF39D2C0);
const _bg = Color(0xFF0E1116);

class CompanionApp extends StatelessWidget {
  const CompanionApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'StageView Companion',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark(useMaterial3: true).copyWith(
        scaffoldBackgroundColor: _bg,
        colorScheme: ColorScheme.fromSeed(
          seedColor: _accent,
          brightness: Brightness.dark,
        ),
      ),
      home: const HomePage(),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final CompanionClient client = CompanionClient();
  // Pre-filled with the PC's LAN IP at build time; editable for any network.
  final TextEditingController hostCtrl = TextEditingController(text: '192.168.0.77');

  @override
  void dispose() {
    client.dispose();
    hostCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: _bg,
        titleSpacing: 16,
        title: const Text('StageView · Monitor Mix',
            style: TextStyle(fontWeight: FontWeight.bold, letterSpacing: 0.5)),
        actions: [
          ListenableBuilder(
            listenable: client,
            builder: (_, _) => Padding(
              padding: const EdgeInsets.only(right: 16),
              child: Row(children: [
                Container(
                  width: 9,
                  height: 9,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: client.connected ? const Color(0xFF39C98C) : const Color(0xFF5A6675),
                  ),
                ),
                const SizedBox(width: 8),
                Text(client.connected ? 'connected' : 'offline',
                    style: const TextStyle(color: Color(0xFF9AA7B4), fontSize: 12)),
              ]),
            ),
          ),
        ],
      ),
      body: Column(children: [
        // --- engine host + connect ---
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 8, 16, 12),
          child: Row(children: [
            Expanded(
              child: TextField(
                controller: hostCtrl,
                keyboardType: const TextInputType.numberWithOptions(decimal: true),
                decoration: const InputDecoration(
                  labelText: 'Engine IP',
                  isDense: true,
                  border: OutlineInputBorder(),
                ),
              ),
            ),
            const SizedBox(width: 10),
            FilledButton(
              onPressed: () => client.connect(hostCtrl.text.trim()),
              child: const Text('Connect'),
            ),
          ]),
        ),
        // --- destination selector + channel faders ---
        Expanded(
          child: ListenableBuilder(
            listenable: client,
            builder: (_, _) {
              if (client.channels.isEmpty) {
                return Center(
                  child: Text(
                    client.connected ? 'waiting for channels...' : 'enter the engine IP and tap Connect',
                    style: const TextStyle(color: Color(0xFF5B6B7A)),
                  ),
                );
              }
              return Column(children: [
                // which output / stereo pair am I mixing?
                SizedBox(
                  height: 46,
                  child: ListView(
                    scrollDirection: Axis.horizontal,
                    padding: const EdgeInsets.symmetric(horizontal: 12),
                    children: [
                      for (final b in client.buses)
                        Padding(
                          padding: const EdgeInsets.only(right: 8, top: 4, bottom: 4),
                          child: ChoiceChip(
                            label: Text(client.busLabel(b)),
                            selected: client.bus == b,
                            onSelected: (_) => client.selectBus(b),
                          ),
                        ),
                    ],
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 0, 8, 4),
                  child: Row(children: [
                    Text('mixing: ${client.busLabel(client.bus)}',
                        style: const TextStyle(color: Color(0xFF7D8B99), fontSize: 12)),
                    const Spacer(),
                    const Text('Stereo pair',
                        style: TextStyle(color: Color(0xFF9AA7B4), fontSize: 12)),
                    Switch(
                      value: client.selectedPairStereo,
                      onChanged: client.setSelectedPairStereo,
                    ),
                  ]),
                ),
                Expanded(
                  child: ListView.builder(
                    padding: const EdgeInsets.only(bottom: 24),
                    itemCount: client.channels.length,
                    itemBuilder: (_, i) {
                      final c = client.channels[i];
                      return ChannelTile(
                        name: c.name.isEmpty ? 'Ch ${i + 1}' : c.name,
                        level: client.valueFor(c),
                        meter: c.meter,
                        onLevel: (v) => client.setValue(i, v),
                      );
                    },
                  ),
                ),
              ]);
            },
          ),
        ),
      ]),
    );
  }
}

/// One source row: name, a live meter bar, and a monitor-send fader.
class ChannelTile extends StatelessWidget {
  const ChannelTile({
    super.key,
    required this.name,
    required this.level,
    required this.meter,
    required this.onLevel,
  });

  final String name;
  final double level;
  final double meter;
  final ValueChanged<double> onLevel;

  Color get _meterColor => meter > 0.85
      ? const Color(0xFFE8514F)
      : meter > 0.6
          ? const Color(0xFFE6B23C)
          : const Color(0xFF39C98C);

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.symmetric(horizontal: 12, vertical: 5),
      padding: const EdgeInsets.fromLTRB(16, 10, 16, 6),
      decoration: BoxDecoration(
        color: const Color(0xFF161B23),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: const Color(0xFF26323F)),
      ),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Row(children: [
          Expanded(
            child: Text(name,
                style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Color(0xFFEAF1F6))),
          ),
          Text('${(level * 100).round()}%',
              style: const TextStyle(color: Color(0xFF7D8B99), fontSize: 12)),
        ]),
        const SizedBox(height: 6),
        // meter bar
        ClipRRect(
          borderRadius: BorderRadius.circular(3),
          child: LinearProgressIndicator(
            value: meter.clamp(0.0, 1.0),
            minHeight: 5,
            backgroundColor: const Color(0xFF0B0E12),
            valueColor: AlwaysStoppedAnimation(_meterColor),
          ),
        ),
        // monitor-send fader
        SliderTheme(
          data: SliderTheme.of(context).copyWith(
            activeTrackColor: _accent,
            thumbColor: _accent,
            inactiveTrackColor: const Color(0xFF26323F),
          ),
          child: Slider(value: level.clamp(0.0, 1.0), onChanged: onLevel),
        ),
      ]),
    );
  }
}
