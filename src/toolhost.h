#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>

#include <functional>

// ToolHost owns a registry of named tool handlers that can be invoked by an
// LLM via Ollama-style tool calls. Handlers are synchronous: they receive a
// JSON object of arguments and return either a result value or an error
// message. ToolHost also exposes the registered tools as a JSON schema array
// in the shape Ollama's /api/chat expects.
class ToolHost : public QObject {
    Q_OBJECT

public:
    using Handler =
        std::function<QJsonValue(const QJsonObject &args, QString *error)>;

    struct ToolSpec {
        QString name;
        QString description;
        QJsonObject parameters; // JSON schema for the arguments object
        Handler handler;
    };

    explicit ToolHost(QObject *parent = nullptr);

    void registerTool(ToolSpec spec);

    // Returns the tools in Ollama's expected shape:
    // [{"type":"function","function":{"name","description","parameters"}}]
    QJsonArray toolsSchema() const;

    // Invokes a registered tool. If the name is unknown or the handler
    // signals an error via its error out-parameter, returns a null value and
    // sets *error. Otherwise returns the handler's result and leaves *error
    // untouched.
    QJsonValue invoke(const QString &name,
                      const QJsonObject &args,
                      QString *error);

private:
    QHash<QString, ToolSpec> m_tools;
};
