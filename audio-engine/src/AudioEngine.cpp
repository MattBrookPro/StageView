#include "AudioEngine.h"

#include "WavFile.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
constexpr float kHalfPi = 1.57079632679f;

int rtCallback(void *out, void * /*in*/, unsigned int nFrames, double /*t*/,
               RtAudioStreamStatus /*status*/, void *userData)
{
    static_cast<AudioEngine *>(userData)->process(static_cast<float *>(out), nFrames);
    return 0;
}

// Linear-interpolation resample, done once at startup (never on the audio thread).
std::vector<float> resampleLinear(const std::vector<float> &in, double inRate, double outRate)
{
    if (in.empty() || inRate == outRate)
        return in;
    const double ratio = outRate / inRate;
    const size_t outN = size_t(in.size() * ratio);
    std::vector<float> out(outN);
    for (size_t i = 0; i < outN; ++i) {
        const double src = i / ratio;
        const size_t i0 = size_t(src);
        const float a = in[i0];
        const float b = (i0 + 1 < in.size()) ? in[i0 + 1] : a;
        out[i] = float(a + (b - a) * (src - double(i0)));
    }
    return out;
}

// Construct an RtAudio bound to a specific API. WASAPI is the safe default (shared,
// coexists with a DAW); ASIO exposes all of an interface's outputs. Guarded so the
// engine still builds on Linux/macOS, where it just uses the platform default.
std::unique_ptr<RtAudio> makeDac(bool asio)
{
#ifdef _WIN32
    return std::make_unique<RtAudio>(asio ? RtAudio::WINDOWS_ASIO : RtAudio::WINDOWS_WASAPI);
#else
    (void)asio;
    return std::make_unique<RtAudio>();
#endif
}
} // namespace

AudioEngine::AudioEngine() = default;
AudioEngine::~AudioEngine() { stop(); }

void AudioEngine::setApi(bool useAsio)
{
    if (m_useAsio != useAsio || !m_dac) {
        m_useAsio = useAsio;
        m_dac.reset(); // rebuilt with the chosen API on next use
    }
}

AudioEngine::Channel *AudioEngine::chan(int i) const
{
    if (i < 0 || i >= int(m_channels.size()))
        return nullptr;
    return m_channels[i].get();
}

QString AudioEngine::channelName(int i) const
{
    Channel *c = chan(i);
    return c ? QString::fromStdString(c->name) : QString();
}

void AudioEngine::refreshStereoSend(Channel &c)
{
    // Outputs 0 & 1 = the stereo main, equal-power panned from level + pan.
    const float angle = (c.pan.load() * 0.5f + 0.5f) * kHalfPi;
    const float lvl = c.level.load();
    c.send[0].store(lvl * std::cos(angle));
    c.send[1].store(lvl * std::sin(angle));
}

bool AudioEngine::load(const QString &stemDir)
{
    QFile f(QDir(stemDir).filePath("stems.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("cannot open %s", qPrintable(f.fileName()));
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    m_sampleRate = root.value("sampleRate").toDouble(44100.0);

    m_channels.clear();
    m_loopLen = 0;
    for (const QJsonValue &v : root.value("channels").toArray()) {
        const QJsonObject o = v.toObject();
        WavFile wav;
        const QString path = QDir(stemDir).filePath(o.value("file").toString());
        if (!wav.load(path.toStdString())) {
            qWarning("failed to load stem %s", qPrintable(path));
            continue;
        }
        auto c = std::make_unique<Channel>();
        c->name = o.value("name").toString().toStdString();
        c->samples = std::move(wav.samples);
        c->level.store(float(o.value("level").toDouble(0.8)));
        c->pan.store(float(o.value("pan").toDouble(0.0)));
        for (auto &s : c->send)
            s.store(0.0f);       // zero the whole send row (atomics aren't zero-init)
        refreshStereoSend(*c);   // then set the stereo main (outputs 0 & 1)
        c->comp.configure(float(m_sampleRate), 10.0f, 120.0f);
        m_loopLen = m_loopLen ? std::min(m_loopLen, c->samples.size()) : c->samples.size();
        m_channels.push_back(std::move(c));
    }

    // Skip the silent count-in so the demo starts on the music; loop that section.
    m_loopStart = 0;
    const size_t step = size_t(m_sampleRate / 10);
    for (size_t p = 0; p + step < m_loopLen; p += step) {
        float mx = 0.0f;
        for (const auto &c : m_channels) {
            const size_t end = std::min(p + 100, c->samples.size());
            for (size_t k = p; k < end; ++k)
                mx = std::max(mx, std::fabs(c->samples[k]));
        }
        if (mx > 0.02f) {
            m_loopStart = p;
            break;
        }
    }
    m_pos.store(m_loopStart);

    qInfo("loaded %d stems (%.1fs each) at %.0f Hz; music starts at %.1fs",
          channelCount(), m_loopLen / m_sampleRate, m_sampleRate, m_loopStart / m_sampleRate);
    return !m_channels.empty();
}

void AudioEngine::listDevices()
{
    if (!m_dac)
        m_dac = makeDac(m_useAsio);
    const unsigned int def = m_dac->getDefaultOutputDevice();
    qInfo("audio API: %s", RtAudio::getApiDisplayName(m_dac->getCurrentApi()).c_str());
    qInfo("output devices:");
    for (const unsigned int id : m_dac->getDeviceIds()) {
        const RtAudio::DeviceInfo info = m_dac->getDeviceInfo(id);
        if (info.outputChannels > 0)
            qInfo("  %s%s (%u outputs, preferred %u Hz)", info.name.c_str(),
                  id == def ? "  [default]" : "", info.outputChannels, info.preferredSampleRate);
    }
}

bool AudioEngine::start(const QString &deviceMatch)
{
    if (m_channels.empty())
        return false;
    if (!m_dac)
        m_dac = makeDac(m_useAsio);

    unsigned int dev = m_dac->getDefaultOutputDevice();
    if (!deviceMatch.isEmpty()) {
        for (const unsigned int id : m_dac->getDeviceIds()) {
            const RtAudio::DeviceInfo info = m_dac->getDeviceInfo(id);
            if (info.outputChannels > 0
                && QString::fromStdString(info.name).contains(deviceMatch, Qt::CaseInsensitive)) {
                dev = id;
                break;
            }
        }
    }
    if (dev == 0) {
        qWarning("no audio output device available");
        return false;
    }
    const RtAudio::DeviceInfo devInfo = m_dac->getDeviceInfo(dev);
    m_outChannels = std::max(2, std::min<int>(int(devInfo.outputChannels), kMaxOutputs));

    // Match the device's native rate (a mismatch under WASAPI shared mode is a
    // common "stream opens but is silent" trap), resampling the stems once.
    const unsigned int devRate = devInfo.preferredSampleRate;
    if (devRate > 0 && double(devRate) != m_sampleRate) {
        const double inRate = m_sampleRate;
        for (auto &c : m_channels) {
            c->samples = resampleLinear(c->samples, inRate, devRate);
            c->low.reset();
            c->mid.reset();
            c->high.reset();
            c->comp.reset();
            c->comp.configure(float(devRate), 10.0f, 120.0f);
        }
        const double scale = double(devRate) / inRate;
        m_loopLen = SIZE_MAX;
        for (const auto &c : m_channels)
            m_loopLen = std::min(m_loopLen, c->samples.size());
        m_loopStart = size_t(double(m_loopStart) * scale);
        m_sampleRate = devRate;
        m_pos.store(m_loopStart);
        qInfo("resampled stems %.0f -> %u Hz to match the device", inRate, devRate);
    }

    RtAudio::StreamParameters params;
    params.deviceId = dev;
    params.nChannels = (unsigned int)m_outChannels;
    params.firstChannel = 0;

    unsigned int bufferFrames = 256;
    RtAudio::StreamOptions opt;
    opt.flags = RTAUDIO_MINIMIZE_LATENCY;
    opt.streamName = "StageView";

    if (m_dac->openStream(&params, nullptr, RTAUDIO_FLOAT32, (unsigned int)m_sampleRate,
                          &bufferFrames, &rtCallback, this, &opt)
        != RTAUDIO_NO_ERROR) {
        qWarning("openStream: %s", m_dac->getErrorText().c_str());
        return false;
    }
    if (m_dac->startStream() != RTAUDIO_NO_ERROR) {
        qWarning("startStream: %s", m_dac->getErrorText().c_str());
        return false;
    }
    m_running = true;
    qInfo("output device: %s (%d outputs)", devInfo.name.c_str(), m_outChannels);
    qInfo("audio stream open: %.0f Hz, %u-frame buffer (~%.1f ms)", m_sampleRate, bufferFrames,
          1000.0 * bufferFrames / m_sampleRate);
    return true;
}

void AudioEngine::stop()
{
    if (m_running && m_dac) {
        if (m_dac->isStreamRunning())
            m_dac->stopStream();
        if (m_dac->isStreamOpen())
            m_dac->closeStream();
        m_running = false;
    }
}

// --- the real-time mix: per source DSP, then distribute to every output ------
void AudioEngine::process(float *out, unsigned int nFrames)
{
    const int N = m_outChannels;
    if (m_loopLen == 0) {
        std::fill(out, out + size_t(nFrames) * N, 0.0f);
        return;
    }

    struct Local {
        Channel *c;
        bool mute, eqOn, compOn;
        float thr, ratio, makeup;
        float level;          // for the meter
        float send[kMaxOutputs];
        float peak;
    };
    constexpr int kMaxSrc = 64;
    Local locals[kMaxSrc];
    const int count = std::min<int>(int(m_channels.size()), kMaxSrc);

    for (int i = 0; i < count; ++i) {
        Channel *c = m_channels[i].get();
        Local &L = locals[i];
        L.c = c;
        L.mute = c->mute.load(std::memory_order_relaxed);
        L.eqOn = c->eqOn.load(std::memory_order_relaxed);
        L.compOn = c->compOn.load(std::memory_order_relaxed);
        L.thr = c->compThreshDb.load(std::memory_order_relaxed);
        L.ratio = c->compRatio.load(std::memory_order_relaxed);
        L.makeup = c->compMakeupDb.load(std::memory_order_relaxed);
        L.level = c->level.load(std::memory_order_relaxed);
        L.peak = 0.0f;
        for (int k = 0; k < N; ++k)
            L.send[k] = c->send[k].load(std::memory_order_relaxed);

        const float fs = float(m_sampleRate);
        c->low.setLowShelf(fs, 120.0f, c->eqLowDb.load(std::memory_order_relaxed), 0.707f);
        c->mid.setPeaking(fs, c->eqMidFreq.load(std::memory_order_relaxed),
                          c->eqMidDb.load(std::memory_order_relaxed), 1.0f);
        c->high.setHighShelf(fs, 6000.0f, c->eqHighDb.load(std::memory_order_relaxed), 0.707f);
    }

    size_t pos = m_pos.load(std::memory_order_relaxed);
    const float master = 0.6f;

    for (unsigned int f = 0; f < nFrames; ++f) {
        float acc[kMaxOutputs] = {0.0f};
        for (int i = 0; i < count; ++i) {
            Local &L = locals[i];
            float s = L.c->samples[pos];           // unity; level lives in the sends
            if (L.eqOn)
                s = L.c->low.process(L.c->mid.process(L.c->high.process(s)));
            if (L.compOn)
                s = L.c->comp.process(s, L.thr, L.ratio, L.makeup);
            const float a = std::fabs(s) * L.level; // meter reflects the main fader
            if (a > L.peak)
                L.peak = a;
            if (!L.mute)
                for (int k = 0; k < N; ++k)
                    acc[k] += s * L.send[k];
        }
        for (int k = 0; k < N; ++k)
            out[size_t(f) * N + k] = std::tanh(acc[k] * master);

        if (++pos >= m_loopLen)
            pos = m_loopStart;
    }

    m_pos.store(pos, std::memory_order_relaxed);
    for (int i = 0; i < count; ++i)
        locals[i].c->meter.store(locals[i].mute ? 0.0f : locals[i].peak,
                                 std::memory_order_relaxed);
}

// --- control plane -----------------------------------------------------------
void AudioEngine::setControl(int channel, const QString &param, double value)
{
    Channel *c = chan(channel);
    if (!c)
        return;
    const float v = float(value);
    if (param == "level") {
        c->level.store(std::clamp(v, 0.0f, 1.0f));
        if (!m_pairSplit[0].load())
            refreshStereoSend(*c); // only drives 1-2 when they're the stereo main
    } else if (param == "pan") {
        c->pan.store(std::clamp(v, -1.0f, 1.0f));
        if (!m_pairSplit[0].load())
            refreshStereoSend(*c);
    } else if (param == "mute") {
        c->mute.store(v != 0.0f);
    } else if (param.startsWith("out/")) {
        const int k = param.mid(4).toInt();
        if (k >= 0 && k < kMaxOutputs)
            c->send[k].store(std::clamp(v, 0.0f, 1.0f)); // independent mono send
    } else if (param == "eq/on")   c->eqOn.store(v != 0.0f);
    else if (param == "eq/low")    c->eqLowDb.store(std::clamp(v, -18.0f, 18.0f));
    else if (param == "eq/mid")    c->eqMidDb.store(std::clamp(v, -18.0f, 18.0f));
    else if (param == "eq/high")   c->eqHighDb.store(std::clamp(v, -18.0f, 18.0f));
    else if (param == "eq/midfreq") c->eqMidFreq.store(std::clamp(v, 200.0f, 8000.0f));
    else if (param == "comp/on")     c->compOn.store(v != 0.0f);
    else if (param == "comp/thresh") c->compThreshDb.store(std::clamp(v, -48.0f, 0.0f));
    else if (param == "comp/ratio")  c->compRatio.store(std::clamp(v, 1.0f, 20.0f));
    else if (param == "comp/makeup") c->compMakeupDb.store(std::clamp(v, 0.0f, 24.0f));
}

double AudioEngine::getControl(int channel, const QString &param) const
{
    Channel *c = chan(channel);
    if (!c)
        return 0.0;
    if (param == "level")        return c->level.load();
    if (param == "pan")          return c->pan.load();
    if (param == "mute")         return c->mute.load() ? 1.0 : 0.0;
    if (param.startsWith("out/")) {
        const int k = param.mid(4).toInt();
        return (k >= 0 && k < kMaxOutputs) ? c->send[k].load() : 0.0;
    }
    if (param == "eq/on")        return c->eqOn.load() ? 1.0 : 0.0;
    if (param == "eq/low")       return c->eqLowDb.load();
    if (param == "eq/mid")       return c->eqMidDb.load();
    if (param == "eq/high")      return c->eqHighDb.load();
    if (param == "eq/midfreq")   return c->eqMidFreq.load();
    if (param == "comp/on")      return c->compOn.load() ? 1.0 : 0.0;
    if (param == "comp/thresh")  return c->compThreshDb.load();
    if (param == "comp/ratio")   return c->compRatio.load();
    if (param == "comp/makeup")  return c->compMakeupDb.load();
    return 0.0;
}

float AudioEngine::meter(int channel) const
{
    Channel *c = chan(channel);
    return c ? c->meter.load(std::memory_order_relaxed) : 0.0f;
}

bool AudioEngine::pairSplit(int p) const
{
    return (p >= 0 && p < kMaxOutputs / 2) && m_pairSplit[p].load();
}

void AudioEngine::setPairSplit(int p, bool split)
{
    if (p < 0 || p >= kMaxOutputs / 2)
        return;
    if (m_pairSplit[p].exchange(split) == split)
        return;
    // Pair 0 returning to stereo: rebuild outputs 0 & 1 from each source's level+pan.
    if (p == 0 && !split)
        for (auto &c : m_channels)
            refreshStereoSend(*c);
}
