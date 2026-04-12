#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
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
    Compositor compositor(&view);

    AppLauncher appLauncher(compositor.socketName());
    ToolHost toolHost;

    // launch_app: ask the compositor to start a known app in its own window.
    {
        ToolHost::ToolSpec spec;
        spec.name = QStringLiteral("launch_app");
        spec.description = QStringLiteral(
            "Launch a desktop application in its own window.");
        QJsonObject nameProp;
        nameProp[QStringLiteral("type")] = QStringLiteral("string");
        nameProp[QStringLiteral("description")] = QStringLiteral(
            "Short app name. Currently supported: clock.");
        QJsonArray nameEnum;
        for (const QString &n : appLauncher.appNames())
            nameEnum.append(n);
        nameProp[QStringLiteral("enum")] = nameEnum;

        QJsonObject properties;
        properties[QStringLiteral("name")] = nameProp;

        QJsonObject parameters;
        parameters[QStringLiteral("type")] = QStringLiteral("object");
        parameters[QStringLiteral("properties")] = properties;
        QJsonArray required;
        required.append(QStringLiteral("name"));
        parameters[QStringLiteral("required")] = required;

        spec.parameters = parameters;
        spec.handler = [&appLauncher](const QJsonObject &args,
                                      QString *error) -> QJsonValue {
            const QString name = args.value(QStringLiteral("name")).toString();
            if (name.isEmpty()) {
                if (error)
                    *error = QStringLiteral("missing required arg: name");
                return {};
            }
            QString err;
            if (!appLauncher.launch(name, &err)) {
                if (error)
                    *error = err;
                return {};
            }
            QJsonObject ok;
            ok[QStringLiteral("ok")] = true;
            ok[QStringLiteral("launched")] = name;
            return ok;
        };
        toolHost.registerTool(std::move(spec));
    }

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
