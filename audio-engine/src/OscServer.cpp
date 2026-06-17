#include "OscServer.h"

#include "AudioEngine.h"
#include "Osc.h"

#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>

namespace {
// Parameters sent in the state snapshot. isInt -> sent as OSC int, else float.
struct ParamSpec {
    const char *path;
    bool isInt;
};
const ParamSpec kSnapshot[] = {
    {"level", false},   {"pan", false},      {"mute", true},
    {"eq/on", true},    {"eq/low", false},   {"eq/mid", false},
    {"eq/high", false}, {"eq/midfreq", false},
    {"comp/on", true},  {"comp/thresh", false}, {"comp/ratio", false}, {"comp/makeup", false},
};
} // namespace

OscServer::OscServer(AudioEngine *engine, quint16 port, QObject *parent)
    : QObject(parent), m_engine(engine), m_socket(new QUdpSocket(this)), m_timer(new QTimer(this))
{
    if (!m_socket->bind(QHostAddress(QHostAddress::AnyIPv4), port))
        qWarning("OSC bind failed on port %u: %s", port, qPrintable(m_socket->errorString()));
    connect(m_socket, &QUdpSocket::readyRead, this, &OscServer::onReadyRead);

    // 50 Hz meter feed - matches the surface's render cadence.
    connect(m_timer, &QTimer::timeout, this, &OscServer::sendMeters);
    m_timer->start(20);
}

void OscServer::sendTo(const QHostAddress &addr, quint16 port, const QByteArray &dg)
{
    m_socket->writeDatagram(dg, addr, port);
}

void OscServer::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_socket->receiveDatagram();
        osc::Message m;
        if (!osc::decode(dg.data(), m))
            continue;

        if (m.address == QLatin1String("/subscribe")) {
            const Sub s{dg.senderAddress(), quint16(dg.senderPort())};
            bool known = false;
            for (const Sub &e : m_subs)
                if (e.addr == s.addr && e.port == s.port)
                    known = true;
            if (!known)
                m_subs.append(s);
            sendSnapshot(s.addr, s.port);
        } else if (m.address == QLatin1String("/unsubscribe")) {
            for (int i = m_subs.size() - 1; i >= 0; --i)
                if (m_subs[i].addr == dg.senderAddress() && m_subs[i].port == dg.senderPort())
                    m_subs.removeAt(i);
        } else if (m.address.startsWith(QLatin1String("/device/pair/"))) {
            // "/device/pair/<p>/split"
            const QStringList parts = m.address.split('/');
            bool ok = false;
            const int p = parts.value(3).toInt(&ok);
            if (ok && parts.size() == 5 && parts.at(4) == QLatin1String("split") && !m.args.isEmpty())
                m_engine->setPairSplit(p, m.args.first().toInt() != 0);
        } else if (m.address.startsWith(QLatin1String("/channel/"))) {
            // "/channel/<n>/<param-path>" - param-path may contain '/', e.g. eq/low.
            const QStringList parts = m.address.split('/');
            bool ok = false;
            const int ch = parts.value(2).toInt(&ok);
            if (ok && parts.size() >= 4 && !m.args.isEmpty()) {
                const QString param = QStringList(parts.mid(3)).join('/');
                m_engine->setControl(ch, param, m.args.first().toDouble());
            }
        }
    }
}

void OscServer::sendSnapshot(const QHostAddress &addr, quint16 port)
{
    const int n = m_engine->channelCount();
    const int outs = m_engine->outputCount();
    sendTo(addr, port, osc::encode("/stage/channels", {QVariant(n)}));
    sendTo(addr, port, osc::encode("/device/outputs", {QVariant(outs)})); // hardware outputs
    for (int p = 0; p < m_engine->pairCount(); ++p)
        sendTo(addr, port, osc::encode(QStringLiteral("/device/pair/%1/split").arg(p),
                                       {QVariant(m_engine->pairSplit(p) ? 1 : 0)}));
    for (int i = 0; i < n; ++i) {
        const QString base = QStringLiteral("/channel/%1/").arg(i);
        sendTo(addr, port, osc::encode(base + "name", {QVariant(m_engine->channelName(i))}));
        for (const ParamSpec &p : kSnapshot) {
            const double val = m_engine->getControl(i, QString::fromLatin1(p.path));
            const QVariant arg = p.isInt ? QVariant(int(val)) : QVariant(float(val));
            sendTo(addr, port, osc::encode(base + QString::fromLatin1(p.path), {arg}));
        }
        // Per-output send levels (the routing-matrix row), so the companion can show
        // the right faders for whichever output it's editing.
        for (int k = 0; k < outs; ++k) {
            const QString p = QStringLiteral("out/%1").arg(k);
            sendTo(addr, port, osc::encode(base + p, {QVariant(float(m_engine->getControl(i, p)))}));
        }
    }
}

void OscServer::sendMeters()
{
    if (m_subs.isEmpty())
        return;
    const int n = m_engine->channelCount();
    QList<QVariant> args;
    args.reserve(n);
    for (int i = 0; i < n; ++i)
        args.append(QVariant(m_engine->meter(i)));
    const QByteArray dg = osc::encode("/meters", args);
    for (const Sub &s : m_subs)
        sendTo(s.addr, s.port, dg);
}
