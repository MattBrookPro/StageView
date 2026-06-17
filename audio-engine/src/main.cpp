// StageView real-time audio engine.
//
// Loads the prepared stems and plays them through RtAudio with per-channel
// zero-latency EQ + compression, controlled by the surface over OSC. This is the
// real engine that replaces the Python mock for the audio demo.
//
//   stageaudio                      # default stems + OSC port 9000
//   stageaudio --stems <dir> --port 9000
//
// Prepare the stems first:  pwsh tools/prepare-stems.ps1

#include "AudioEngine.h"
#include "OscServer.h"

#include <QCoreApplication>

#include <cstdio>

// Route Qt logging to stderr with an explicit flush. A console app's qInfo output
// otherwise goes to the debugger (not the console) when stdout/stderr are
// redirected, which hides startup progress.
static void msgHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    std::fprintf(stderr, "%s\n", qPrintable(msg));
    std::fflush(stderr);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(msgHandler);
    QCoreApplication app(argc, argv);

    // Default to the cache tools/prepare-stems.ps1 writes.
    QString stemDir = qEnvironmentVariable("LOCALAPPDATA") + "/StageView/stems";
    quint16 port = 9000;
    QString device; // substring match for the output device (default if empty)
    bool listDevices = false;
    bool asio = false; // ASIO exposes all of an interface's outputs (Windows)

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == "--stems" && i + 1 < args.size())
            stemDir = args.at(++i);
        else if (args.at(i) == "--port" && i + 1 < args.size())
            port = args.at(++i).toUShort();
        else if (args.at(i) == "--device" && i + 1 < args.size())
            device = args.at(++i);
        else if (args.at(i) == "--list-devices")
            listDevices = true;
        else if (args.at(i) == "--asio")
            asio = true;
    }

    if (listDevices) {
        AudioEngine e;
        e.setApi(asio);
        e.listDevices();
        return 0;
    }

    AudioEngine engine;
    engine.setApi(asio);
    if (!engine.load(stemDir)) {
        qCritical("No stems loaded from %s - run tools/prepare-stems.ps1 first.",
                  qPrintable(stemDir));
        return 1;
    }

    OscServer server(&engine, port); // control plane up before audio starts

    if (!engine.start(device)) {
        qCritical("Failed to start the audio stream.");
        return 2;
    }

    qInfo("StageView audio engine: %d channels, OSC on udp:%u. Ctrl+C to stop.",
          engine.channelCount(), port);
    return app.exec();
}
