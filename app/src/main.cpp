// StageView - entry point.
//
// This is the one place the C++ <-> QML plumbing lives: it creates the StageModel
// (which spins up the network worker thread and connects to the engine), exposes
// it to QML as the `Stage` context property, and loads the root window.
//
// Flags:
//   --host <addr>        engine host (default 127.0.0.1)
//   --port <n>           engine UDP port (default 9000)
//   --shot <file>        grab the window to a PNG after a delay, then quit
//   --shot-delay <ms>    delay before the grab (default 1500)
// The --shot path is used for the README image, a headless smoke test, and quick
// visual verification; it also exercises Qt's offscreen rendering.

#include "StageModel.h"

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QtQuickControls2/QQuickStyle>

#include <cstdio>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("StageView");
    app.setOrganizationName("MattBrook");

    // The "Basic" style stays out of the way so the stage canvas defines the look.
    QQuickStyle::setStyle("Basic");

    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 9000;
    QString shotPath;
    int shotDelay = 1500;

    const QStringList a = app.arguments();
    for (int i = 1; i < a.size(); ++i) {
        if (a.at(i) == "--host" && i + 1 < a.size())
            host = a.at(++i);
        else if (a.at(i) == "--port" && i + 1 < a.size())
            port = a.at(++i).toUShort();
        else if (a.at(i) == "--shot" && i + 1 < a.size())
            shotPath = a.at(++i);
        else if (a.at(i) == "--shot-delay" && i + 1 < a.size())
            shotDelay = a.at(++i).toInt();
    }

    QQmlApplicationEngine engine;

    StageModel stage(host, port);
    engine.rootContext()->setContextProperty(QStringLiteral("Stage"), &stage);

    // If the QML module fails to load, exit non-zero so CI/headless runs catch it
    // instead of hanging on an empty event loop.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // loadFromModule() resolves "Main" from the StageView module that
    // qt_add_qml_module compiled into the binary - no filesystem paths.
    engine.loadFromModule("StageView", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    if (!shotPath.isEmpty()) {
        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(shotDelay, &app, [win, shotPath, &app]() {
            bool ok = false;
            if (win) {
                const QImage img = win->grabWindow();
                ok = !img.isNull() && img.save(shotPath);
            }
            // stderr (not qInfo) so the line always flushes before we quit.
            std::fprintf(stderr, "stageview: screenshot %s -> %s\n",
                         qPrintable(shotPath), ok ? "saved" : "FAILED");
            std::fflush(stderr);
            app.quit();
        });
    }

    return app.exec();
}
