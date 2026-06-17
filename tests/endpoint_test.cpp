// Integration test for OscEndpoint - the threaded UDP transport.
//
// This is the test that actually exercises the "money milestone": an endpoint
// running on a real worker thread, talking to a stand-in engine socket, with data
// crossing the thread boundary back to this (the main/test) thread as a queued
// signal. We assert both directions:
//   * start() makes the endpoint subscribe (UI thread -> worker -> socket -> wire)
//   * an inbound /meters datagram surfaces as a meters() signal with the right
//     values on this thread (wire -> socket -> worker -> queued -> here)
//   * sendControl() puts the expected OSC message on the wire
//
// Written with Qt Test + QSignalSpy, the standard Qt testing tools.

#include "Osc.h"
#include "OscEndpoint.h"

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QThread>
#include <QUdpSocket>
#include <QtTest>

class EndpointTest : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void subscribesOnStart();
    void receivesMetersAcrossThread();
    void sendsControlToEngine();

private:
    // A stand-in for the engine: a plain socket on the test thread we can read
    // from and reply on, just like mock_engine.py does.
    QUdpSocket *m_engine = nullptr;
    quint16 m_enginePort = 0;
    QThread *m_worker = nullptr;
    OscEndpoint *m_endpoint = nullptr;

    // Read one datagram the endpoint sent us, decoding it. Records the sender so
    // replies go back to the endpoint's bound port.
    bool readFromEndpoint(osc::Message &out, QHostAddress &from, quint16 &fromPort);
};

void EndpointTest::init()
{
    m_engine = new QUdpSocket(this);
    QVERIFY(m_engine->bind(QHostAddress(QHostAddress::LocalHost), 0));
    m_enginePort = m_engine->localPort();

    m_worker = new QThread(this);
    m_endpoint = new OscEndpoint; // no parent -> lives on the worker thread
    m_endpoint->moveToThread(m_worker);
    m_worker->start();
}

void EndpointTest::cleanup()
{
    QMetaObject::invokeMethod(m_endpoint, "stop", Qt::BlockingQueuedConnection);
    m_worker->quit();
    m_worker->wait();
    delete m_endpoint;
    m_endpoint = nullptr;
    // m_worker and m_engine are parented to this and cleaned up by QObject.
    delete m_worker;
    m_worker = nullptr;
    delete m_engine;
    m_engine = nullptr;
}

bool EndpointTest::readFromEndpoint(osc::Message &out, QHostAddress &from, quint16 &fromPort)
{
    if (!m_engine->hasPendingDatagrams())
        return false;
    const QNetworkDatagram dg = m_engine->receiveDatagram();
    from = dg.senderAddress();
    fromPort = dg.senderPort();
    return osc::decode(dg.data(), out);
}

void EndpointTest::subscribesOnStart()
{
    QSignalSpy inbound(m_engine, &QUdpSocket::readyRead);
    QMetaObject::invokeMethod(m_endpoint, "start", Qt::QueuedConnection,
                              Q_ARG(QString, QStringLiteral("127.0.0.1")),
                              Q_ARG(quint16, m_enginePort));
    QVERIFY(inbound.wait(2000));

    osc::Message m;
    QHostAddress from;
    quint16 fromPort = 0;
    QVERIFY(readFromEndpoint(m, from, fromPort));
    QCOMPARE(m.address, QStringLiteral("/subscribe"));
}

void EndpointTest::receivesMetersAcrossThread()
{
    // Start + consume the subscribe so we know the endpoint's reply address.
    QSignalSpy inbound(m_engine, &QUdpSocket::readyRead);
    QMetaObject::invokeMethod(m_endpoint, "start", Qt::QueuedConnection,
                              Q_ARG(QString, QStringLiteral("127.0.0.1")),
                              Q_ARG(quint16, m_enginePort));
    QVERIFY(inbound.wait(2000));
    osc::Message sub;
    QHostAddress endpointAddr;
    quint16 endpointPort = 0;
    QVERIFY(readFromEndpoint(sub, endpointAddr, endpointPort));

    // Now play the engine: send a meter frame back to the endpoint.
    QSignalSpy meterSpy(m_endpoint, &OscEndpoint::meters);
    const QByteArray frame = osc::encode(
        "/meters", {QVariant(0.10f), QVariant(0.20f), QVariant(0.30f)});
    m_engine->writeDatagram(frame, endpointAddr, endpointPort);

    QVERIFY(meterSpy.wait(2000));            // crossed the thread boundary
    QCOMPARE(meterSpy.count(), 1);
    const auto levels = meterSpy.at(0).at(0).value<QVector<float>>();
    QCOMPARE(levels.size(), 3);
    QCOMPARE(levels.at(0), 0.10f);
    QCOMPARE(levels.at(1), 0.20f);
    QCOMPARE(levels.at(2), 0.30f);
}

void EndpointTest::sendsControlToEngine()
{
    QSignalSpy inbound(m_engine, &QUdpSocket::readyRead);
    QMetaObject::invokeMethod(m_endpoint, "start", Qt::QueuedConnection,
                              Q_ARG(QString, QStringLiteral("127.0.0.1")),
                              Q_ARG(quint16, m_enginePort));
    QVERIFY(inbound.wait(2000));
    osc::Message sub;
    QHostAddress a;
    quint16 p = 0;
    QVERIFY(readFromEndpoint(sub, a, p)); // discard the subscribe

    // Ask the endpoint (on its thread) to send a control change.
    QMetaObject::invokeMethod(m_endpoint, "sendControl", Qt::QueuedConnection,
                              Q_ARG(int, 3), Q_ARG(QString, QStringLiteral("level")),
                              Q_ARG(double, 0.5));
    QVERIFY(inbound.wait(2000));

    osc::Message ctrl;
    QVERIFY(readFromEndpoint(ctrl, a, p));
    QCOMPARE(ctrl.address, QStringLiteral("/channel/3/level"));
    QCOMPARE(ctrl.args.size(), 1);
    QCOMPARE(ctrl.args.at(0).toFloat(), 0.5f);
}

// GUILESS: this test is pure networking/threads, so a QCoreApplication is enough.
// It also means the test needs no display - it runs on headless CI as-is.
QTEST_GUILESS_MAIN(EndpointTest)
#include "endpoint_test.moc"
