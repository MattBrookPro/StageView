#include "MidiInput.h"

#include "MidiMapper.h"

#include <RtMidi.h>

#include <QtGlobal>

MidiInput::MidiInput(QObject *parent)
    : QObject(parent)
{
    try {
        m_in = std::make_unique<RtMidiIn>();
    } catch (const RtMidiError &e) {
        // No MIDI subsystem available - keep going without MIDI rather than crash.
        qWarning("MIDI unavailable: %s", e.what());
        m_in.reset();
    }
}

MidiInput::~MidiInput()
{
    closePort();
}

QStringList MidiInput::availablePorts() const
{
    QStringList ports;
    if (!m_in)
        return ports;
    try {
        const unsigned int count = m_in->getPortCount();
        for (unsigned int i = 0; i < count; ++i)
            ports << QString::fromStdString(m_in->getPortName(i));
    } catch (const RtMidiError &) {
        // ignore - return whatever we gathered
    }
    return ports;
}

bool MidiInput::openPort(unsigned int index)
{
    if (!m_in)
        return false;
    try {
        if (m_in->getPortCount() <= index)
            return false; // no port to open (no controller plugged in)
        // We only care about channel-voice messages; drop sysex/timing/sensing so
        // the callback isn't woken for clock ticks etc.
        m_in->ignoreTypes(true, true, true);
        m_in->setCallback(&MidiInput::onMidiStatic, this);
        m_in->openPort(index);
        m_portName = QString::fromStdString(m_in->getPortName(index));
        emit portOpened(m_portName);
        return true;
    } catch (const RtMidiError &e) {
        qWarning("MIDI open failed: %s", e.what());
        return false;
    }
}

void MidiInput::closePort()
{
    // Only tear down if we actually opened a port - avoids RtMidi's "no callback
    // was set" warning on the common no-controller path.
    if (m_in && m_in->isPortOpen()) {
        try {
            m_in->cancelCallback();
            m_in->closePort();
        } catch (const RtMidiError &) {
            // nothing to do
        }
    }
    m_portName.clear();
}

void MidiInput::onMidiStatic(double, std::vector<unsigned char> *message, void *userData)
{
    if (message && userData)
        static_cast<MidiInput *>(userData)->onMidi(*message);
}

void MidiInput::onMidi(const std::vector<unsigned char> &message)
{
    if (message.size() < 3)
        return; // channel-voice messages we care about are 3 bytes
    const midi::Action a = midi::mapMessage(message[0], message[1], message[2]);
    if (a.valid)
        emit controlReceived(a.source, a.param, a.value); // queued onto the UI thread
}
