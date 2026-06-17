#pragma once

// Per-channel DSP: a biquad EQ band and a feed-forward compressor.
//
// Both are deliberately ZERO-added-latency: the biquads are minimum-phase (no
// linear-phase FIR), and the compressor has no lookahead. That is the right choice
// for a live console - you trade the "perfect" offline behaviour for not delaying
// the signal at all. process() does no allocation and no locking, so it is safe to
// call from the real-time audio callback.

#include <cmath>

namespace dsp {

inline float db2lin(float db) { return std::pow(10.0f, db * 0.05f); }
inline float lin2db(float x) { return 20.0f * std::log10(x); }

// Transposed Direct-Form II biquad. RBJ-cookbook coefficient setters.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    inline float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }

    void setPeaking(float fs, float f, float gainDb, float q);
    void setLowShelf(float fs, float f, float gainDb, float q);
    void setHighShelf(float fs, float f, float gainDb, float q);
    void setBypass();
};

// Simple feed-forward peak compressor with smoothed envelope and make-up gain.
struct Compressor {
    void configure(float fs, float attackMs, float releaseMs);

    // thresholdDb / ratio / makeupDb are read fresh each block by the caller.
    inline float process(float x, float thresholdDb, float ratio, float makeupDb)
    {
        const float in = std::fabs(x);
        // One-pole envelope: fast attack when rising, slow release when falling.
        const float a = (in > m_env) ? m_aAtt : m_aRel;
        m_env = a * m_env + (1.0f - a) * in;

        const float envDb = lin2db(m_env + 1e-9f);
        const float overDb = envDb - thresholdDb;
        // Above threshold, reduce by (1 - 1/ratio) of the overshoot (negative dB).
        const float grDb = (overDb > 0.0f) ? overDb * (1.0f / ratio - 1.0f) : 0.0f;
        return x * db2lin(grDb + makeupDb);
    }
    void reset() { m_env = 0.0f; }

private:
    float m_env = 0.0f;
    float m_aAtt = 0.0f;
    float m_aRel = 0.0f;
};

} // namespace dsp
