#include "WavFile.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>

namespace {
uint32_t rd32(const unsigned char *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint16_t rd16(const unsigned char *p)
{
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
} // namespace

bool WavFile::load(const std::string &path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return false;
    const std::streamsize size = f.tellg(); // bulk read - byte-by-byte is far too slow for ~17 MB
    f.seekg(0);
    std::vector<unsigned char> buf(size > 0 ? size_t(size) : 0);
    if (buf.size() < 44 || !f.read(reinterpret_cast<char *>(buf.data()), size))
        return false;
    if (std::memcmp(buf.data(), "RIFF", 4) != 0 || std::memcmp(buf.data() + 8, "WAVE", 4) != 0)
        return false;

    uint16_t fmt = 0;
    int bits = 0;
    const unsigned char *data = nullptr;
    uint32_t dataSize = 0;

    // Walk the RIFF chunks; each is a 4-byte id + 4-byte size + payload (padded to even).
    size_t pos = 12;
    while (pos + 8 <= buf.size()) {
        const unsigned char *ch = buf.data() + pos;
        const uint32_t sz = rd32(ch + 4);
        if (std::memcmp(ch, "fmt ", 4) == 0 && sz >= 16) {
            fmt = rd16(ch + 8);
            channels = rd16(ch + 10);
            sampleRate = int(rd32(ch + 12));
            bits = rd16(ch + 22);
        } else if (std::memcmp(ch, "data", 4) == 0) {
            data = ch + 8;
            dataSize = sz;
        }
        pos += 8 + sz + (sz & 1);
    }
    if (!data || channels < 1)
        return false;

    if (fmt == 1 && bits == 16) { // PCM 16-bit
        const size_t n = dataSize / 2;
        samples.resize(n);
        for (size_t i = 0; i < n; ++i) {
            int16_t s;
            std::memcpy(&s, data + i * 2, 2); // memcpy avoids unaligned-load UB
            samples[i] = float(s) / 32768.0f;
        }
        return true;
    }
    if (fmt == 3 && bits == 32) { // IEEE float
        const size_t n = dataSize / 4;
        samples.resize(n);
        std::memcpy(samples.data(), data, n * 4);
        return true;
    }
    return false; // unsupported format
}
