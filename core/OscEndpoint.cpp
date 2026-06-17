#include "OscEndpoint.h"

#include "Osc.h"

#include <QNetworkDatagram>
#include <QUdpSocket>

OscEndpoint::OscEndpoint(QObject *parent)
    : QObject(parent)
{
    // QVector<float> isn't a built-in queued-connection type, so register it once.
    // Without this, the cross-thread meters() signal would fail to marshal at runtime.
    qRegisterMetaType<QVector<float>>("QVector<float>");
}

OscEndpoint::~OscEndpoint() = default;

void OscEndpoint::start(const QString &engineHost, quint16 enginePort)
{
    // Created here (not in the constructor) so the socket's thread affinity is the
    // WORKER thread that runs start(), satisfying QUdpSocket's single-thread rule.
    if (!m_socket) {
        m_socket = new QUdpSocket(this);
        connect(m_socket, &QUdpSocket::readyRead, this, &OscEndpoint::onReadyRead);
    }

    m_engineAddr = QHostAddress(engineHost);
    if (m_engineAddr.isNull())
        m_engineAddr = QHostAddress(QHostAddress::LocalHost);
    m_enginePort = enginePort;

    // Bind an OS-assigned ephemeral port. We send control *and* receive meters on
    // this one socket; the engine replies to whatever address it heard from, so a
    // single socket per side is all the protocol needs (and it is NAT-friendly).
    if (m_socket->state() != QAbstractSocket::BoundState) {
        m_socket->bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    }

    // Subscribe: from now on the engine streams meters to us.
    send(osc::encode("/subscribe"));

    if (!m_connected) {
        m_connected = true;
        emit connectedChanged(true);
    }
}

void OscEndpoint::stop()
{
    if (m_socket && m_socket->state() == QAbstractSocket::BoundState)
        send(osc::encode("/unsubscribe"));
    if (m_socket) {
        m_socket->close();
    }
    if (m_connected) {
        m_connected = false;
        emit connectedChanged(false);
    }
}

void OscEndpoint::sendControl(int channel, const QString &param, double value)
{
    const QString address = QStringLiteral("/channel/%1/%2").arg(channel).arg(param);
    // mute and the eq/comp on-switches are integers; everything else is a float32.
    const bool isInt = param == QLatin1String("mute") || param.endsWith(QLatin1String("/on"));
    if (isInt)
        send(osc::encode(address, {QVariant(static_cast<int>(value))}));
    else
        send(osc::encode(address, {QVariant(static_cast<float>(value))}));
}

void OscEndpoint::send(const QByteArray &datagram)
{
    if (m_socket)
        m_socket->writeDatagram(datagram, m_engineAddr, m_enginePort);
}

void OscEndpoint::onReadyRead()
{
    // Drain every datagram the worker thread's event loop just woke us for.
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_socket->receiveDatagram();
        osc::Message msg;
        if (!osc::decode(dg.data(), msg))
            continue; // ignore anything that isn't well-formed OSC

        if (msg.address == QLatin1String("/meters")) {
            QVector<float> levels;
            levels.reserve(msg.args.size());
            for (const QVariant &a : msg.args)
                levels.append(a.toFloat());
            emit meters(levels); // queued -> copied onto the UI thread's event loop
        } else if (msg.address == QLatin1String("/stage/channels")) {
            if (!msg.args.isEmpty())
                emit channelCount(msg.args.first().toInt());
        } else if (msg.address.startsWith(QLatin1String("/channel/"))) {
            // "/channel/<n>/<param-path>" - param path may be multi-segment (eq/low).
            const QStringList parts = msg.address.split('/');
            bool ok = false;
            const int idx = parts.value(2).toInt(&ok);
            if (ok && parts.size() >= 4 && !msg.args.isEmpty())
                emit channelParam(idx, QStringList(parts.mid(3)).join('/'), msg.args.first());
        }
    }
}
