#pragma once

// OscEndpoint - the UDP socket that talks to the audio engine.
//
// This is the crux of the whole exercise: meter data arrives from the engine at
// ~50 Hz and must reach the UI without ever stalling the 60 fps render. The design:
//
//   * The endpoint is built to live on its OWN worker thread. Every socket
//     operation - binding, reading, writing - happens on that thread's event loop.
//     The GUI thread therefore never does blocking network I/O.
//
//   * Inbound data crosses back to the UI as Qt signals. Because the endpoint and
//     the UI objects live on different threads, Qt delivers those signals via a
//     QUEUED connection: the argument is *copied* into the receiver's event queue
//     and the slot runs on the UI thread. No shared mutable state crosses the
//     boundary, so there is nothing to lock. A queued signal/slot is the right tool
//     at meter rate; at audio rate you would reach for a lock-free ring buffer
//     instead.
//
// Threading contract (important):
//   - Construct it, moveToThread(worker), start the worker's event loop.
//   - Invoke start()/stop()/sendControl() only via queued calls (a normal
//     signal->slot from the UI thread does this automatically). They MUST run on
//     the worker thread, because a QUdpSocket may only be touched by the thread
//     that created it.

#include <QHostAddress>
#include <QObject>
#include <QVariant>
#include <QVector>
#include <cstdint>

class QUdpSocket;

class OscEndpoint : public QObject
{
    Q_OBJECT
public:
    explicit OscEndpoint(QObject *parent = nullptr);
    ~OscEndpoint() override;

public slots:
    // Create+bind the socket (on the worker thread) and subscribe to the engine.
    void start(const QString &engineHost, quint16 enginePort);
    // Unsubscribe and tear the socket down.
    void stop();
    // Send one control parameter: (3, "level", 0.75) -> "/channel/3/level" 0.75f.
    void sendControl(int channel, const QString &param, double value);

signals:
    // Emitted from the worker thread; received on the UI thread via a queued
    // connection. These four are the entire surface<-engine vocabulary.
    void channelCount(int count);
    void channelParam(int channel, const QString &param, const QVariant &value);
    void meters(const QVector<float> &levels);
    void connectedChanged(bool connected);

private slots:
    void onReadyRead();

private:
    void send(const QByteArray &datagram);

    QUdpSocket *m_socket = nullptr;
    QHostAddress m_engineAddr;
    quint16 m_enginePort = 0;
    bool m_connected = false;
};
