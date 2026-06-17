#include "StageModel.h"

#include "MidiInput.h"
#include "OscEndpoint.h"

#include <QMetaObject>

StageModel::StageModel(QString host, quint16 port, QObject *parent)
    : QAbstractListModel(parent)
{
    m_endpoint = new OscEndpoint; // no parent: it will live on the worker thread
    m_endpoint->moveToThread(&m_thread);

    // When the thread ends, delete the endpoint on that thread - the safe place.
    connect(&m_thread, &QThread::finished, m_endpoint, &QObject::deleteLater);

    // engine -> UI: queued automatically because sender (endpoint) and receiver
    // (this) live on different threads. The payloads are copied across; no locks.
    connect(m_endpoint, &OscEndpoint::channelCount, this, &StageModel::onChannelCount);
    connect(m_endpoint, &OscEndpoint::channelParam, this, &StageModel::onChannelParam);
    connect(m_endpoint, &OscEndpoint::meters, this, &StageModel::onMeters);
    connect(m_endpoint, &OscEndpoint::connectedChanged, this, &StageModel::onConnectedChanged);

    // UI -> engine: queued the other way, so the socket is only ever used on the
    // worker thread (QUdpSocket's hard requirement).
    connect(this, &StageModel::startRequested, m_endpoint, &OscEndpoint::start);
    connect(this, &StageModel::controlRequested, m_endpoint, &OscEndpoint::sendControl);

    m_thread.start();
    // Delivered once the worker's event loop spins up - binds the socket + subscribes.
    emit startRequested(host, port);

    // MIDI input lives on this (the UI) thread; RtMidi runs its own callback thread
    // internally and controlReceived crosses back queued. Auto-open the first port
    // if a controller is present; absence is fine and silent.
    m_midi = new MidiInput(this);
    connect(m_midi, &MidiInput::controlReceived, this, &StageModel::onMidiControl);
    connect(m_midi, &MidiInput::portOpened, this, [this](const QString &name) {
        m_midiPort = name;
        emit midiPortChanged();
    });
    m_midi->openPort(0);
}

StageModel::~StageModel()
{
    // Politely unsubscribe and close the socket on the worker thread before we
    // tear the thread down. Blocking-queued is safe: we're on the UI thread and
    // the endpoint is on the worker, so there is no self-deadlock.
    if (m_endpoint)
        QMetaObject::invokeMethod(m_endpoint, "stop", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

int StageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_sources.size();
}

QVariant StageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sources.size())
        return {};
    const Source &s = m_sources.at(index.row());
    switch (role) {
    case NameRole:  return s.name;
    case LevelRole: return s.level;
    case PanRole:   return s.pan;
    case MuteRole:  return s.mute;
    case MeterRole: return s.meter;
    default:        return {};
    }
}

QHash<int, QByteArray> StageModel::roleNames() const
{
    return {
        {NameRole,  "name"},
        {LevelRole, "level"},
        {PanRole,   "pan"},
        {MuteRole,  "mute"},
        {MeterRole, "meter"},
    };
}

void StageModel::applyControl(int row, const QString &param, double value)
{
    if (row < 0 || row >= m_sources.size())
        return;
    Source &s = m_sources[row];
    int role = -1;
    double sent = value;
    if (param == QLatin1String("level")) {
        s.level = static_cast<float>(qBound(0.0, value, 1.0));
        role = LevelRole;
        sent = s.level;
    } else if (param == QLatin1String("pan")) {
        s.pan = static_cast<float>(qBound(-1.0, value, 1.0));
        role = PanRole;
        sent = s.pan;
    } else if (param == QLatin1String("mute")) {
        s.mute = value != 0.0;
        role = MuteRole;
        sent = s.mute ? 1.0 : 0.0;
    }
    if (role < 0)
        return;

    // Update the model (the puck moves / dims) and push to the engine (queued onto
    // the worker thread). Every input - drag, mute toggle, MIDI - funnels here.
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {role});
    emit controlRequested(row, param, sent);
}

void StageModel::setLevelPan(int row, double level, double pan)
{
    applyControl(row, QStringLiteral("level"), level);
    applyControl(row, QStringLiteral("pan"), pan);
}

void StageModel::setMute(int row, bool mute)
{
    applyControl(row, QStringLiteral("mute"), mute ? 1.0 : 0.0);
}

void StageModel::onMidiControl(int source, const QString &param, double value)
{
    applyControl(source, param, value);
}

void StageModel::setSelectedChannel(int i)
{
    if (i < -1 || i >= m_sources.size())
        i = -1;
    if (m_selected == i)
        return;
    m_selected = i;
    emit selectedChannelChanged();
}

double StageModel::getParam(int row, const QString &param) const
{
    if (row < 0 || row >= m_sources.size())
        return 0.0;
    const Source &s = m_sources.at(row);
    if (param == QLatin1String("level"))
        return s.level;
    if (param == QLatin1String("pan"))
        return s.pan;
    if (param == QLatin1String("mute"))
        return s.mute ? 1.0 : 0.0;
    if (param == QLatin1String("meter"))
        return s.meter;
    return s.dsp.value(param, 0.0);
}

void StageModel::setParam(int row, const QString &param, double value)
{
    if (row < 0 || row >= m_sources.size())
        return;
    // level/pan/mute have model roles + positioning; route through applyControl.
    if (param == QLatin1String("level") || param == QLatin1String("pan")
        || param == QLatin1String("mute")) {
        applyControl(row, param, value);
        return;
    }
    // DSP params: store locally and forward to the engine (queued onto the worker).
    m_sources[row].dsp.insert(param, value);
    emit controlRequested(row, param, value);
}

QString StageModel::nameOf(int row) const
{
    if (row < 0 || row >= m_sources.size())
        return {};
    return m_sources.at(row).name;
}

void StageModel::ensureRows(int count)
{
    if (count <= m_sources.size())
        return;
    beginInsertRows(QModelIndex(), m_sources.size(), count - 1);
    while (m_sources.size() < count)
        m_sources.append(Source{});
    endInsertRows();
}

void StageModel::onChannelCount(int count)
{
    if (count < 0)
        return;
    if (count < m_sources.size()) {
        beginRemoveRows(QModelIndex(), count, m_sources.size() - 1);
        m_sources.resize(count);
        endRemoveRows();
    } else {
        ensureRows(count);
    }
}

void StageModel::onChannelParam(int channel, const QString &param, const QVariant &value)
{
    if (channel < 0)
        return;
    ensureRows(channel + 1); // tolerate a param arriving before /stage/channels
    Source &s = m_sources[channel];
    int role = -1;
    if (param == QLatin1String("name")) {
        s.name = value.toString();
        role = NameRole;
    } else if (param == QLatin1String("level")) {
        s.level = value.toFloat();
        role = LevelRole;
    } else if (param == QLatin1String("pan")) {
        s.pan = value.toFloat();
        role = PanRole;
    } else if (param == QLatin1String("mute")) {
        s.mute = value.toInt() != 0;
        role = MuteRole;
    } else {
        // EQ/compressor params (eq/low, comp/ratio, ...) - the engine sends these in
        // the snapshot; the control panel reads them back via getParam().
        s.dsp.insert(param, value.toDouble());
    }
    if (role != -1) {
        const QModelIndex idx = index(channel);
        emit dataChanged(idx, idx, {role});
    }
}

void StageModel::onMeters(const QVector<float> &levels)
{
    // The UI-thread end of the meter handoff: copy the latest levels into the
    // model and signal one contiguous change. Cheap for a handful of channels at
    // ~50 Hz, and it never blocks the render because we're already on the UI thread.
    const int n = qMin(levels.size(), m_sources.size());
    for (int i = 0; i < n; ++i)
        m_sources[i].meter = levels.at(i);
    if (n > 0)
        emit dataChanged(index(0), index(n - 1), {MeterRole});
}

void StageModel::onConnectedChanged(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}
