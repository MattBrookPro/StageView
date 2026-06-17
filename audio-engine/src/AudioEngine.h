#pragma once

// AudioEngine - loads the stems and plays them through RtAudio with per-channel
// zero-latency EQ + compression and a per-output SEND MATRIX (independent monitor
// mixes per hardware output).
//
// Mix model: each source is processed once (EQ + compressor), then distributed to
// every hardware output through its own send gain:
//     out[k] = sum_s  dsp(source_s) * send[s][k]
// The "stereo main" is outputs 0 & 1, driven by a source's level + pan. Any other
// output (or output 0/1 individually) is an independent mono monitor mix the
// companion app can dial in. That's how a real personal-monitor system works.
//
// Threading model is unchanged: the RtAudio callback is real-time (no alloc/locks),
// reads params via std::atomic; the OSC control plane writes them.

#include "Dsp.h"

#include <RtAudio.h>

#include <QString>

#include <atomic>
#include <memory>
#include <vector>

inline constexpr int kMaxOutputs = 32; // send-matrix width cap (per source)

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    void setApi(bool useAsio); // ASIO exposes all of an interface's outputs (Windows)

    bool load(const QString &stemDir);
    bool start(const QString &deviceMatch = QString());
    void stop();
    void listDevices();

    int channelCount() const { return int(m_channels.size()); }
    QString channelName(int i) const;
    int outputCount() const { return m_outChannels; } // hardware outputs in use

    // Each adjacent output pair can be a stereo bus or two independent mono buses;
    // nothing is hardwired. pair p = outputs (2p, 2p+1).
    int pairCount() const { return m_outChannels / 2; }
    bool pairSplit(int p) const;
    void setPairSplit(int p, bool split);

    // Apply one control parameter (from the OSC thread):
    //   "level","pan","mute","eq/*","comp/*"  - as before (level/pan drive the
    //                                            stereo main, outputs 0 & 1)
    //   "out/<k>"                              - source's mono send to output k
    void setControl(int channel, const QString &param, double value);
    double getControl(int channel, const QString &param) const;
    float meter(int channel) const;

    void process(float *out, unsigned int nFrames); // real-time mix (public for the trampoline)

private:
    struct Channel {
        std::string name;
        std::vector<float> samples;

        std::atomic<float> level{0.8f}; // stereo-main fader
        std::atomic<float> pan{0.0f};
        std::atomic<bool> mute{false};

        std::atomic<bool> eqOn{true};
        std::atomic<float> eqLowDb{0.0f};
        std::atomic<float> eqMidDb{0.0f};
        std::atomic<float> eqHighDb{0.0f};
        std::atomic<float> eqMidFreq{1000.0f};

        std::atomic<bool> compOn{false};
        std::atomic<float> compThreshDb{-18.0f};
        std::atomic<float> compRatio{2.0f};
        std::atomic<float> compMakeupDb{0.0f};

        std::atomic<float> meter{0.0f};

        // Per-output send gains (the routing matrix row for this source).
        std::atomic<float> send[kMaxOutputs];

        dsp::Biquad low, mid, high; // audio-thread-only DSP state
        dsp::Compressor comp;
    };

    Channel *chan(int i) const;
    void refreshStereoSend(Channel &c); // recompute send[0],[1] from level + pan

    std::vector<std::unique_ptr<Channel>> m_channels;
    std::atomic<size_t> m_pos{0};
    size_t m_loopStart = 0;
    size_t m_loopLen = 0;
    double m_sampleRate = 44100.0;
    int m_outChannels = 2;
    bool m_useAsio = false;
    // Per output-pair: false = stereo, true = two mono outs. Pair 0 (outputs 1-2)
    // is the desktop's spatial main when stereo (level/pan drives it).
    std::atomic<bool> m_pairSplit[kMaxOutputs / 2] = {};

    std::unique_ptr<RtAudio> m_dac;
    bool m_running = false;
};
