#pragma once

// OscServer - the engine's control plane. A UDP socket on the Qt event loop that
// speaks the same OSC protocol the mock engine did, so the existing surface drives
// the real audio with no changes:
//   surface -> engine   /subscribe, /channel/<n>/<param> <value>
//   engine  -> surface   /stage/channels, /channel/<n>/<param> snapshot, /meters
// New params (eq/*, comp/*) ride the same /channel/<n>/<path> scheme.
//
// This lives on the control thread; it only ever touches the engine through its
// atomic-backed setControl()/getControl()/meter(), never the audio callback.

#include <QHostAddress>
#include <QList>
#include <QObject>

class QUdpSocket;
class QTimer;
class AudioEngine;

class OscServer : public QObject
{
    Q_OBJECT
public:
    OscServer(AudioEngine *engine, quint16 port, QObject *parent = nullptr);

private slots:
    void onReadyRead();
    void sendMeters();

private:
    struct Sub {
        QHostAddress addr;
        quint16 port;
    };
    void sendSnapshot(const QHostAddress &addr, quint16 port);
    void sendTo(const QHostAddress &addr, quint16 port, const QByteArray &datagram);

    AudioEngine *m_engine;
    QUdpSocket *m_socket;
    QTimer *m_timer;
    QList<Sub> m_subs;
};
