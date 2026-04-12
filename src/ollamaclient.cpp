#include "ollamaclient.h"
#include "chatmodel.h"
#include "toolhost.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkRequest>

namespace {

// Extract a QJsonObject from a tool-call arguments field, which Ollama/qwen
// may deliver as either a parsed object or a JSON-encoded string.
QJsonObject parseToolArgs(const QJsonValue &v)
{
    if (v.isObject())
        return v.toObject();
    if (v.isString()) {
        const QByteArray bytes = v.toString().toUtf8();
        QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (doc.isObject())
            return doc.object();
    }
    return {};
}

QString compactJson(const QJsonValue &v)
{
    if (v.isObject())
        return QString::fromUtf8(
            QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isArray())
        return QString::fromUtf8(
            QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(v.toDouble());
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isNull())
        return QStringLiteral("null");
    return {};
}

} // namespace

OllamaClient::OllamaClient(ChatModel *model, ToolHost *tools, QObject *parent)
    : QObject(parent)
    , m_chatModel(model)
    , m_tools(tools)
    , m_systemPrompt(QStringLiteral(
          "You are Botnik, an LLM-driven desktop assistant that controls a "
          "Wayland compositor via tool calls. When the user asks you to do "
          "something that matches an available tool, call the tool instead "
          "of describing what you would do. Only respond in plain text for "
          "questions or conversation that don't map to a tool. Do not "
          "narrate tool calls before making them."))
    , m_modelName(QStringLiteral("qwen2.5:7b"))
{
    connect(m_chatModel, &ChatModel::userMessageAdded, this, &OllamaClient::send);
}

void OllamaClient::send(const QString &userMessage)
{
    Q_UNUSED(userMessage);

    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    m_chatModel->addAssistantMessage();
    m_currentAssistant = {};
    m_awaitingToolCalls = false;
    m_toolRounds = 0;

    seedMessagesFromHistory();
    postChat();
}

void OllamaClient::seedMessagesFromHistory()
{
    m_messages = {};

    QJsonObject sys;
    sys[QStringLiteral("role")] = QStringLiteral("system");
    sys[QStringLiteral("content")] = m_systemPrompt;
    m_messages.append(sys);

    const auto &history = m_chatModel->messages();
    for (const auto &msg : history) {
        // Skip the empty assistant bubble we just added for the in-flight
        // response — it will be populated by streamed tokens and committed
        // into m_messages as m_currentAssistant at tool-call time.
        if (msg.role == QStringLiteral("assistant") && msg.content.isEmpty())
            continue;
        QJsonObject obj;
        obj[QStringLiteral("role")] = msg.role;
        obj[QStringLiteral("content")] = msg.content;
        m_messages.append(obj);
    }
}

void OllamaClient::postChat()
{
    QJsonObject body;
    body[QStringLiteral("model")] = m_modelName;
    body[QStringLiteral("messages")] = m_messages;
    body[QStringLiteral("stream")] = true;
    if (m_tools) {
        const QJsonArray schema = m_tools->toolsSchema();
        if (!schema.isEmpty())
            body[QStringLiteral("tools")] = schema;
    }

    QNetworkRequest req(QUrl(QStringLiteral("http://localhost:11434/api/chat")));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));

    m_reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &OllamaClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &OllamaClient::onFinished);
}

void OllamaClient::onReadyRead()
{
    while (m_reply->canReadLine()) {
        QByteArray line = m_reply->readLine().trimmed();
        if (line.isEmpty())
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isNull())
            continue;

        QJsonObject obj = doc.object();
        QJsonObject message = obj[QStringLiteral("message")].toObject();

        const QString token = message[QStringLiteral("content")].toString();
        if (!token.isEmpty()) {
            m_chatModel->appendToLastMessage(token);
            m_currentAssistant[QStringLiteral("content")] =
                m_currentAssistant[QStringLiteral("content")].toString() + token;
        }

        const QJsonArray toolCalls =
            message[QStringLiteral("tool_calls")].toArray();
        if (!toolCalls.isEmpty()) {
            QJsonArray accumulated =
                m_currentAssistant[QStringLiteral("tool_calls")].toArray();
            for (const QJsonValue &tc : toolCalls)
                accumulated.append(tc);
            m_currentAssistant[QStringLiteral("tool_calls")] = accumulated;
            m_awaitingToolCalls = true;
        }
    }
}

void OllamaClient::onFinished()
{
    if (!m_reply)
        return;

    // Drain any trailing buffered bytes that didn't end in a newline.
    QByteArray remaining = m_reply->readAll().trimmed();
    if (!remaining.isEmpty()) {
        for (const QByteArray &line : remaining.split('\n')) {
            QByteArray trimmed = line.trimmed();
            if (trimmed.isEmpty())
                continue;
            QJsonDocument doc = QJsonDocument::fromJson(trimmed);
            if (doc.isNull())
                continue;
            QJsonObject obj = doc.object();
            QJsonObject message = obj[QStringLiteral("message")].toObject();

            const QString token = message[QStringLiteral("content")].toString();
            if (!token.isEmpty()) {
                m_chatModel->appendToLastMessage(token);
                m_currentAssistant[QStringLiteral("content")] =
                    m_currentAssistant[QStringLiteral("content")].toString()
                    + token;
            }

            const QJsonArray toolCalls =
                message[QStringLiteral("tool_calls")].toArray();
            if (!toolCalls.isEmpty()) {
                QJsonArray accumulated =
                    m_currentAssistant[QStringLiteral("tool_calls")].toArray();
                for (const QJsonValue &tc : toolCalls)
                    accumulated.append(tc);
                m_currentAssistant[QStringLiteral("tool_calls")] = accumulated;
                m_awaitingToolCalls = true;
            }
        }
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_awaitingToolCalls) {
        handleToolCallsAndContinue();
    }
}

void OllamaClient::handleToolCallsAndContinue()
{
    // Commit the assistant message (content + tool_calls) to the history.
    QJsonObject assistantMsg = m_currentAssistant;
    assistantMsg[QStringLiteral("role")] = QStringLiteral("assistant");
    // Ensure content key exists even if empty, some servers care.
    if (!assistantMsg.contains(QStringLiteral("content")))
        assistantMsg[QStringLiteral("content")] = QString();
    m_messages.append(assistantMsg);

    const QJsonArray toolCalls =
        m_currentAssistant[QStringLiteral("tool_calls")].toArray();

    for (const QJsonValue &tcVal : toolCalls) {
        const QJsonObject tc = tcVal.toObject();
        const QJsonObject function = tc[QStringLiteral("function")].toObject();
        const QString name = function[QStringLiteral("name")].toString();
        const QJsonObject args =
            parseToolArgs(function[QStringLiteral("arguments")]);

        const QString argsJson = QString::fromUtf8(
            QJsonDocument(args).toJson(QJsonDocument::Compact));
        m_chatModel->appendToLastMessage(
            QStringLiteral("\n[tool %1 %2]").arg(name, argsJson));

        QJsonValue result;
        QString resultString;
        if (!m_tools) {
            resultString = QStringLiteral("{\"error\":\"no tool host\"}");
            m_chatModel->appendToLastMessage(
                QStringLiteral("\n[\u2192 err: no tool host]"));
        } else {
            QString err;
            result = m_tools->invoke(name, args, &err);
            if (!err.isEmpty()) {
                QJsonObject obj;
                obj[QStringLiteral("error")] = err;
                resultString = QString::fromUtf8(
                    QJsonDocument(obj).toJson(QJsonDocument::Compact));
                m_chatModel->appendToLastMessage(
                    QStringLiteral("\n[\u2192 err: %1]").arg(err));
            } else {
                resultString = compactJson(result);
                m_chatModel->appendToLastMessage(
                    QStringLiteral("\n[\u2192 ok]"));
            }
        }

        QJsonObject toolMsg;
        toolMsg[QStringLiteral("role")] = QStringLiteral("tool");
        toolMsg[QStringLiteral("name")] = name;
        toolMsg[QStringLiteral("content")] = resultString;
        m_messages.append(toolMsg);
    }

    // Reset and prepare for the follow-up round.
    m_currentAssistant = {};
    m_awaitingToolCalls = false;
    ++m_toolRounds;

    if (m_toolRounds >= kMaxToolRounds) {
        m_chatModel->appendToLastMessage(
            QStringLiteral("\n[tool loop bound reached, stopping]"));
        return;
    }

    // Fresh assistant bubble for the model's follow-up text.
    m_chatModel->addAssistantMessage();
    postChat();
}
