#pragma once

// MidiMapper - translate a raw MIDI message into a StageView control action.
//
// This is the cross-platform heart of MIDI support: pure logic, no RtMidi, no
// platform code, so it unit-tests in isolation (tests/midi_test.cpp). The actual
// port I/O is a thin, swappable backend (app/src/MidiInput) that calls into this.
//
// Mapping (standard MIDI CC semantics, so real controllers "just work"):
//   CC7  (Channel Volume) on MIDI channel N  ->  source (N-1) level = d2/127   [0..1]
//   CC10 (Pan)            on MIDI channel N  ->  source (N-1) pan, 64 = centre  [-1..1]
// The MIDI channel selects which source, so a bank-per-source controller maps
// cleanly. Anything else (notes, other CCs, system messages) yields an invalid
// action and is ignored.

#include <QString>
#include <cstdint>

namespace midi {

struct Action {
    bool valid = false;
    int source = -1;       // which channel/source this targets
    QString param;         // "level" | "pan"
    double value = 0.0;    // level 0..1, pan -1..1
};

Action mapMessage(uint8_t status, uint8_t data1, uint8_t data2);

} // namespace midi
