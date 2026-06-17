// Tests for the MIDI -> action mapper. Framework-free, like osc_test: the value
// is that the cross-platform mapping logic is verified without needing a MIDI
// device or the RtMidi backend at all.

#include "MidiMapper.h"

#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static bool nearly(double a, double b) { return (a - b < 1e-9) && (b - a < 1e-9); }

static void test_cc7_is_level()
{
    // CC7 on MIDI channel 1 (status 0xB0) -> source 0 level.
    midi::Action a = midi::mapMessage(0xB0, 7, 127);
    CHECK(a.valid);
    CHECK(a.source == 0);
    CHECK(a.param == "level");
    CHECK(nearly(a.value, 1.0));

    // CC7 on MIDI channel 3 (status 0xB2), value 0 -> source 2 level 0.
    midi::Action b = midi::mapMessage(0xB2, 7, 0);
    CHECK(b.valid);
    CHECK(b.source == 2);
    CHECK(b.param == "level");
    CHECK(nearly(b.value, 0.0));
}

static void test_cc10_is_pan_centered()
{
    // 64 -> exactly centre.
    midi::Action c = midi::mapMessage(0xB0, 10, 64);
    CHECK(c.valid);
    CHECK(c.param == "pan");
    CHECK(nearly(c.value, 0.0));

    // 0 -> hard left (clamped to -1).
    CHECK(nearly(midi::mapMessage(0xB0, 10, 0).value, -1.0));
    // 127 -> hard right (+1).
    CHECK(nearly(midi::mapMessage(0xB0, 10, 127).value, 1.0));
    // channel selects the source.
    CHECK(midi::mapMessage(0xB5, 10, 64).source == 5);
}

static void test_irrelevant_messages_ignored()
{
    CHECK(!midi::mapMessage(0x90, 60, 100).valid); // Note On
    CHECK(!midi::mapMessage(0x80, 60, 0).valid);   // Note Off
    CHECK(!midi::mapMessage(0xB0, 11, 64).valid);  // CC11 (not mapped)
    CHECK(!midi::mapMessage(0xF8, 0, 0).valid);    // timing clock
}

int main()
{
    test_cc7_is_level();
    test_cc10_is_pan_centered();
    test_irrelevant_messages_ignored();

    if (g_failures == 0) {
        std::printf("midi_test: all checks passed\n");
        return 0;
    }
    std::printf("midi_test: %d check(s) failed\n", g_failures);
    return 1;
}
