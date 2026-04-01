#include <QGuiApplication>
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

    ChatModel chatModel;
    OllamaClient ollama(&chatModel);

    view.rootContext()->setContextProperty(QStringLiteral("chatModel"), &chatModel);
    view.setSource(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    view.resize(1024, 768);
    view.show();

    Compositor compositor(&view);

    return app.exec();
}
