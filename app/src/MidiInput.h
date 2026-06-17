#pragma once

// MidiInput - the RtMidi-backed MIDI input port.
//
// This is the thin, platform-specific backend; all the actual interpretation lives
// in the cross-platform midi::mapMessage (core/MidiMapper). RtMidi delivers messages
// on its own internal thread via a callback; we translate and re-emit as a Qt signal.
// Because the receiver (StageModel) lives on the UI thread and the callback runs on
// RtMidi's thread, Qt delivers that signal queued - the same safe cross-thread handoff
// the network side uses, reached a second way.
//
// If there is no MIDI subsystem (e.g. a headless CI runner) construction degrades
// gracefully: the object simply reports no ports and emits nothing.

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

class RtMidiIn;

class MidiInput : public QObject
{
    Q_OBJECT
public:
    explicit MidiInput(QObject *parent = nullptr);
    ~MidiInput() override;

    QString portName() const { return m_portName; }
    QStringList availablePorts() const;

public slots:
    bool openPort(unsigned int index = 0); // true if a port was opened
    void closePort();

signals:
    void controlReceived(int source, const QString &param, double value);
    void portOpened(const QString &name);

private:
    static void onMidiStatic(double timeStamp, std::vector<unsigned char> *message, void *userData);
    void onMidi(const std::vector<unsigned char> &message);

    std::unique_ptr<RtMidiIn> m_in;
    QString m_portName;
};
