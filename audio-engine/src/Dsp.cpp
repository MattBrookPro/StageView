#include "Dsp.h"

#include <algorithm> // std::max (MSVC needs this explicitly; libstdc++ pulls it in)

namespace dsp {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

// RBJ Audio-EQ-Cookbook formulas. a0 is divided out so process() needs only
// b0,b1,b2,a1,a2.

void Biquad::setPeaking(float fs, float f, float gainDb, float q)
{
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * f / fs;
    const float cw = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * q);

    const float a0 = 1.0f + alpha / A;
    b0 = (1.0f + alpha * A) / a0;
    b1 = (-2.0f * cw) / a0;
    b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * cw) / a0;
    a2 = (1.0f - alpha / A) / a0;
}

void Biquad::setLowShelf(float fs, float f, float gainDb, float q)
{
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * f / fs;
    const float cw = std::cos(w0);
    const float sw = std::sin(w0);
    const float alpha = sw / (2.0f * q);
    const float tsa = 2.0f * std::sqrt(A) * alpha;

    const float a0 = (A + 1.0f) + (A - 1.0f) * cw + tsa;
    b0 = (A * ((A + 1.0f) - (A - 1.0f) * cw + tsa)) / a0;
    b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw)) / a0;
    b2 = (A * ((A + 1.0f) - (A - 1.0f) * cw - tsa)) / a0;
    a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cw)) / a0;
    a2 = ((A + 1.0f) + (A - 1.0f) * cw - tsa) / a0;
}

void Biquad::setHighShelf(float fs, float f, float gainDb, float q)
{
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * kPi * f / fs;
    const float cw = std::cos(w0);
    const float sw = std::sin(w0);
    const float alpha = sw / (2.0f * q);
    const float tsa = 2.0f * std::sqrt(A) * alpha;

    const float a0 = (A + 1.0f) - (A - 1.0f) * cw + tsa;
    b0 = (A * ((A + 1.0f) + (A - 1.0f) * cw + tsa)) / a0;
    b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw)) / a0;
    b2 = (A * ((A + 1.0f) + (A - 1.0f) * cw - tsa)) / a0;
    a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cw)) / a0;
    a2 = ((A + 1.0f) - (A - 1.0f) * cw - tsa) / a0;
}

void Biquad::setBypass()
{
    b0 = 1.0f;
    b1 = b2 = a1 = a2 = 0.0f;
}

void Compressor::configure(float fs, float attackMs, float releaseMs)
{
    // One-pole smoothing coefficient a = exp(-1 / (time_seconds * fs)).
    m_aAtt = std::exp(-1.0f / (std::max(0.1f, attackMs) * 0.001f * fs));
    m_aRel = std::exp(-1.0f / (std::max(1.0f, releaseMs) * 0.001f * fs));
}

} // namespace dsp
