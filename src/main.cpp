#include <QGuiApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlContext>
#include <QQuickView>
#include <QTimer>

#include "compositor.h"
#include "chatmodel.h"
#include "ollamaclient.h"

namespace {

QProcess *launchClock(QObject *parent, const QString &socketName)
{
    auto *clock = new QProcess(parent);
    clock->setProgram(QGuiApplication::applicationDirPath()
                      + QStringLiteral("/botnik-clock"));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), socketName);
    env.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_WAYLAND_DISABLE_WINDOWDECORATION"), QStringLiteral("1"));
    clock->setProcessEnvironment(env);
    clock->start();
    return clock;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1024, 768);

    ChatModel chatModel;
    OllamaClient ollama(&chatModel);
    Compositor compositor(&view);

    view.rootContext()->setContextProperty(QStringLiteral("chatModel"), &chatModel);
    view.rootContext()->setContextProperty(QStringLiteral("workspaceModel"),
                                          compositor.workspaceModel());
    view.rootContext()->setContextProperty(QStringLiteral("compositor"), &compositor);

    view.setSource(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    view.show();

    // Launch the clock client as a Wayland client of this compositor.
    QProcess *clock = launchClock(&app, compositor.socketName());

    // Optional second clock for manual testing of workspace switching.
    // Off by default; set BOTNIK_EXTRA_CLOCK=1 to enable. Delayed so the
    // live active-workspace swap is observable.
    QProcess *extraClock = nullptr;
    if (qEnvironmentVariableIntValue("BOTNIK_EXTRA_CLOCK") > 0) {
        QTimer::singleShot(1500, &app, [&app, &compositor, &extraClock]() {
            extraClock = launchClock(&app, compositor.socketName());
        });
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app,
                     [&clock, &extraClock]() {
        for (QProcess *p : {clock, extraClock}) {
            if (p && p->state() != QProcess::NotRunning) {
                p->kill();
                p->waitForFinished(500);
            }
        }
    });

    return app.exec();
}
