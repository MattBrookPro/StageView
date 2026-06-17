#include "MidiMapper.h"

#include <QtGlobal>

namespace midi {

Action mapMessage(uint8_t status, uint8_t data1, uint8_t data2)
{
    Action a;
    const uint8_t type = status & 0xF0; // high nibble = message type
    const int channel = status & 0x0F;  // low nibble = MIDI channel (0-based)

    if (type == 0xB0) { // Control Change
        const int d2 = data2 & 0x7F;    // MIDI data bytes are 7-bit
        if (data1 == 7) {               // CC7  = Channel Volume -> level
            a.valid = true;
            a.source = channel;
            a.param = QStringLiteral("level");
            a.value = d2 / 127.0;
        } else if (data1 == 10) {       // CC10 = Pan (64 = centre) -> pan
            a.valid = true;
            a.source = channel;
            a.param = QStringLiteral("pan");
            // Centre (64) maps to exactly 0; ends clamp to -1 / +1.
            a.value = qBound(-1.0, (d2 - 64) / 63.0, 1.0);
        }
    }
    return a;
}

} // namespace midi
