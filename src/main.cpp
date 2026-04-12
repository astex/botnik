#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlContext>
#include <QQuickView>

#include "applauncher.h"
#include "chatmodel.h"
#include "compositor.h"
#include "ollamaclient.h"
#include "toolhost.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

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
    view.rootContext()->setContextProperty(QStringLiteral("surfaceModel"),
                                          compositor.surfaceModel());

    view.setSource(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    view.show();

    // Launch the clock client as a Wayland client of this compositor.
    // This auto-launched instance is slice B's test fixture — do not remove
    // or relocate. Tool-driven launches spawn additional clock windows.
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

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [clock, &appLauncher]() {
        if (clock->state() != QProcess::NotRunning) {
            clock->kill();
            clock->waitForFinished(500);
        }
        appLauncher.shutdown();
    });

    return app.exec();
}
