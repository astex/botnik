#include "toolhost.h"

ToolHost::ToolHost(QObject *parent)
    : QObject(parent)
{
}

void ToolHost::registerTool(ToolSpec spec)
{
    const QString name = spec.name;
    m_tools.insert(name, std::move(spec));
}

QJsonArray ToolHost::toolsSchema() const
{
    QJsonArray arr;
    for (auto it = m_tools.cbegin(); it != m_tools.cend(); ++it) {
        const ToolSpec &spec = it.value();
        QJsonObject function;
        function[QStringLiteral("name")] = spec.name;
        function[QStringLiteral("description")] = spec.description;
        function[QStringLiteral("parameters")] = spec.parameters;

        QJsonObject entry;
        entry[QStringLiteral("type")] = QStringLiteral("function");
        entry[QStringLiteral("function")] = function;
        arr.append(entry);
    }
    return arr;
}

QJsonValue ToolHost::invoke(const QString &name,
                            const QJsonObject &args,
                            QString *error)
{
    auto it = m_tools.constFind(name);
    if (it == m_tools.cend()) {
        if (error)
            *error = QStringLiteral("unknown tool: %1").arg(name);
        return {};
    }
    QString localErr;
    QJsonValue result = it.value().handler(args, &localErr);
    if (!localErr.isEmpty()) {
        if (error)
            *error = localErr;
        return {};
    }
    return result;
}
