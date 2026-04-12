#include <QGuiApplication>
#include <QProcess>
#include <QQmlContext>
#include <QQuickView>

#include "compositor.h"
#include "headlesscompositor.h"
#include "chatmodel.h"
#include "ollamaclient.h"
#include "stdinreader.h"

static bool isHeadless(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--headless") == 0)
            return true;
    }
    return qEnvironmentVariableIsSet("BOTNIK_HEADLESS");
}

static void removeHeadlessArg(int &argc, char *argv[])
{
    int dst = 1;
    for (int src = 1; src < argc; ++src) {
        if (qstrcmp(argv[src], "--headless") != 0)
            argv[dst++] = argv[src];
    }
    argc = dst;
}

static int runHeadless(QGuiApplication &app)
{
    ChatModel chatModel;
    OllamaClient ollama(&chatModel);
    HeadlessCompositor compositor;

    StdinReader stdinReader(&chatModel);

    // Launch the clock client against the headless compositor.
    auto *clock = new QProcess(&app);
    clock->setProgram(QGuiApplication::applicationDirPath()
                      + QStringLiteral("/botnik-clock"));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), compositor.socketName());
    env.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_WAYLAND_DISABLE_WINDOWDECORATION"), QStringLiteral("1"));
    clock->setProcessEnvironment(env);
    clock->start();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [clock]() {
        if (clock->state() != QProcess::NotRunning) {
            clock->kill();
            clock->waitForFinished(500);
        }
    });

    return app.exec();
}

static int runGui(QGuiApplication &app)
{
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1024, 768);

    ChatModel chatModel;
    OllamaClient ollama(&chatModel);
    Compositor compositor(&view);

    view.rootContext()->setContextProperty(QStringLiteral("chatModel"), &chatModel);
    view.rootContext()->setContextProperty(QStringLiteral("surfaceModel"),
                                          compositor.surfaceModel());

    view.setSource(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    view.show();

    // Launch the clock client as a Wayland client of this compositor.
    auto *clock = new QProcess(&app);
    clock->setProgram(QGuiApplication::applicationDirPath()
                      + QStringLiteral("/botnik-clock"));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), compositor.socketName());
    env.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_WAYLAND_DISABLE_WINDOWDECORATION"), QStringLiteral("1"));
    clock->setProcessEnvironment(env);
    clock->start();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [clock]() {
        if (clock->state() != QProcess::NotRunning) {
            clock->kill();
            clock->waitForFinished(500);
        }
    });

    return app.exec();
}

int main(int argc, char *argv[])
{
    bool headless = isHeadless(argc, argv);

    if (headless) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        removeHeadlessArg(argc, argv);
    }

    QGuiApplication app(argc, argv);

    if (headless)
        return runHeadless(app);
    else
        return runGui(app);
}
