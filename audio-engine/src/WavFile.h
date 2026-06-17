#pragma once

// A tiny WAV reader for the engine's stems. We control the format (ffmpeg writes
// mono 16-bit PCM at 44.1 kHz in tools/prepare-stems.ps1), so this only needs to
// handle 16-bit PCM and 32-bit float - no external decoder dependency required.

#include <string>
#include <vector>

struct WavFile {
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> samples; // interleaved; mono for our stems

    // Returns false on a missing file or an unsupported/!PCM format.
    bool load(const std::string &path);
};
