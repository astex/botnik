#include <QGuiApplication>
#include <QProcess>
#include <QQmlContext>
#include <QQuickView>

#include "compositor.h"
#include "chatmodel.h"
#include "ollamaclient.h"

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
