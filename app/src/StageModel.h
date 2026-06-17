#pragma once

// StageModel - the bridge between the network endpoint and the QML view.
//
// It is a QAbstractListModel of audio channels. Crucially it stores *audio*
// parameters (level/pan/mute) plus a live meter - NOT stage x/y. The "drag a
// source around the stage" mapping (y<->level, x<->pan) lives entirely in QML, so
// the model stays a faithful mirror of engine state and the spatial idea is a pure
// view-layer reinterpretation.
//
// It also owns the worker thread the OscEndpoint runs on, and wires the two
// directions of traffic:
//   engine -> model : endpoint signals arrive here via QUEUED connections (they
//                     hop threads), and we mutate the model on the UI thread only.
//   model  -> engine: control changes are emitted as signals connected to the
//                     endpoint's slots, so the socket is always touched on its own
//                     thread. The UI never calls the socket directly.

#include <QAbstractListModel>
#include <QHash>
#include <QThread>
#include <QVector>

class OscEndpoint;
class MidiInput;

class StageModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString midiPort READ midiPort NOTIFY midiPortChanged)
    // Which source the control panel is editing (-1 = none).
    Q_PROPERTY(int selectedChannel READ selectedChannel WRITE setSelectedChannel NOTIFY selectedChannelChanged)
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        LevelRole, // 0..1   - the surface maps vertical position to this
        PanRole,   // -1..1  - the surface maps horizontal position to this
        MuteRole,
        MeterRole, // 0..1   - live level streamed from the engine
    };
    Q_ENUM(Roles)

    explicit StageModel(QString host = QStringLiteral("127.0.0.1"),
                        quint16 port = 9000, QObject *parent = nullptr);
    ~StageModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool connected() const { return m_connected; }
    QString midiPort() const { return m_midiPort; }
    int selectedChannel() const { return m_selected; }
    void setSelectedChannel(int i);

    // Called from QML as the user drags a source. We update local state at once
    // (so the UI feels instant) and push the new params to the engine.
    Q_INVOKABLE void setLevelPan(int row, double level, double pan);
    Q_INVOKABLE void setMute(int row, bool mute);

    // Generic accessors for the DSP parameters the control panel edits
    // (e.g. "eq/low", "comp/thresh", "comp/on"). level/pan/mute also work.
    Q_INVOKABLE double getParam(int row, const QString &param) const;
    Q_INVOKABLE void setParam(int row, const QString &param, double value);
    Q_INVOKABLE QString nameOf(int row) const;

signals:
    void connectedChanged();
    void midiPortChanged();
    void selectedChannelChanged();

    // Emitted on the UI thread, received on the worker thread -> queued, so these
    // are the safe way to drive the socket. (Declared as signals purely to get
    // automatic thread marshalling to the endpoint's slots.)
    void startRequested(const QString &host, quint16 port);
    void controlRequested(int channel, const QString &param, double value);

private slots:
    // The engine->UI side of the handoff. All run on the UI thread.
    void onChannelCount(int count);
    void onChannelParam(int channel, const QString &param, const QVariant &value);
    void onMeters(const QVector<float> &levels);
    void onConnectedChanged(bool connected);
    // A MIDI controller moved something - drive the same path a drag does.
    void onMidiControl(int source, const QString &param, double value);

private:
    struct Source {
        QString name;
        float level = 0.7f;
        float pan = 0.0f;
        bool mute = false;
        float meter = 0.0f;
        // EQ/compressor params keyed by name ("eq/low", "comp/ratio", ...). The
        // panel reads/writes these; the engine is the source of their defaults.
        QHash<QString, double> dsp;
    };

    void ensureRows(int count); // grow the model to at least `count` rows
    // Single place that applies one parameter change locally and forwards it to
    // the engine - shared by drag (QML), mute, and MIDI, so all inputs are equal.
    void applyControl(int row, const QString &param, double value);

    QVector<Source> m_sources;
    QThread m_thread;
    OscEndpoint *m_endpoint = nullptr;
    MidiInput *m_midi = nullptr;
    bool m_connected = false;
    QString m_midiPort;
    int m_selected = -1;
};
