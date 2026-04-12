#include <QGuiApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlContext>
#include <QQuickView>
#include <QTimer>

#include "applauncher.h"
#include "chatmodel.h"
#include "compositor.h"
#include "headlesscompositor.h"
#include "ollamaclient.h"
#include "stdinreader.h"
#include "toolhost.h"
#include "windowtools.h"

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

static bool isHeadless(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--headless") == 0)
            return true;
    }
    return qEnvironmentVariableIsSet("BOTNIK_HEADLESS");
}

static QString parseSocketName(int argc, char *argv[])
{
    for (int i = 1; i < argc - 1; ++i) {
        if (qstrcmp(argv[i], "--socket-name") == 0)
            return QString::fromLocal8Bit(argv[i + 1]);
    }
    return {};
}

static void removeCustomArgs(int &argc, char *argv[])
{
    int dst = 1;
    for (int src = 1; src < argc; ++src) {
        if (qstrcmp(argv[src], "--headless") == 0)
            continue;
        if (qstrcmp(argv[src], "--socket-name") == 0) {
            ++src; // skip the value too
            continue;
        }
        argv[dst++] = argv[src];
    }
    argc = dst;
}

static bool s_suppressEgl = false;
static QtMessageHandler s_defaultHandler = nullptr;

static void headlessMessageHandler(QtMsgType type, const QMessageLogContext &ctx,
                                   const QString &msg)
{
    if (s_suppressEgl && type == QtWarningMsg
        && msg.contains(QStringLiteral("Failed to initialize EGL"))) {
        return;
    }
    if (s_defaultHandler)
        s_defaultHandler(type, ctx, msg);
}

static int runHeadless(QGuiApplication &app, const QString &socketName)
{
    ChatModel chatModel;
    HeadlessCompositor compositor(socketName);

    AppLauncher appLauncher(compositor.socketName());
    ToolHost toolHost;

    for (auto &spec : appLauncher.toolSpecs())
        toolHost.registerTool(std::move(spec));
    for (auto &spec : windowToolSpecs(compositor.workspaceModel()))
        toolHost.registerTool(std::move(spec));

    OllamaClient ollama(&chatModel, &toolHost);

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

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app,
                     [clock, &appLauncher]() {
        if (clock->state() != QProcess::NotRunning) {
            clock->kill();
            clock->waitForFinished(500);
        }
        appLauncher.shutdown();
    });

    return app.exec();
}

static int runGui(QGuiApplication &app)
{
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1024, 768);

    ChatModel chatModel;
    Compositor compositor(&view);

    AppLauncher appLauncher(compositor.socketName());
    ToolHost toolHost;

    for (auto &spec : appLauncher.toolSpecs())
        toolHost.registerTool(std::move(spec));
    for (auto &spec : windowToolSpecs(compositor.workspaceModel()))
        toolHost.registerTool(std::move(spec));

    OllamaClient ollama(&chatModel, &toolHost);

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
                     [&clock, &extraClock, &appLauncher]() {
        for (QProcess *p : {clock, extraClock}) {
            if (p && p->state() != QProcess::NotRunning) {
                p->kill();
                p->waitForFinished(500);
            }
        }
        appLauncher.shutdown();
    });

    return app.exec();
}

int main(int argc, char *argv[])
{
    bool headless = isHeadless(argc, argv);
    QString socketName = parseSocketName(argc, argv);

    if (headless) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        s_suppressEgl = true;
        s_defaultHandler = qInstallMessageHandler(headlessMessageHandler);
        removeCustomArgs(argc, argv);
    }

    QGuiApplication app(argc, argv);

    if (headless)
        return runHeadless(app, socketName);
    else
        return runGui(app);
}
