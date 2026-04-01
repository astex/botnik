#include "ollamaclient.h"
#include "chatmodel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

OllamaClient::OllamaClient(ChatModel *model, QObject *parent)
    : QObject(parent)
    , m_chatModel(model)
    , m_systemPrompt(QStringLiteral("You are Botnik, a desktop assistant."))
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

    QJsonObject body;
    body[QStringLiteral("model")] = m_modelName;
    body[QStringLiteral("messages")] = buildMessages();
    body[QStringLiteral("stream")] = true;

    QNetworkRequest req(QUrl(QStringLiteral("http://localhost:11434/api/chat")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

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
        QString token = obj[QStringLiteral("message")]
                            .toObject()[QStringLiteral("content")]
                            .toString();
        if (!token.isEmpty())
            m_chatModel->appendToLastMessage(token);
    }
}

void OllamaClient::onFinished()
{
    if (m_reply) {
        // Process any remaining data not ending in newline
        QByteArray remaining = m_reply->readAll().trimmed();
        if (!remaining.isEmpty()) {
            for (const QByteArray &line : remaining.split('\n')) {
                QByteArray trimmed = line.trimmed();
                if (trimmed.isEmpty())
                    continue;
                QJsonDocument doc = QJsonDocument::fromJson(trimmed);
                if (doc.isNull())
                    continue;
                QString token = doc.object()[QStringLiteral("message")]
                                    .toObject()[QStringLiteral("content")]
                                    .toString();
                if (!token.isEmpty())
                    m_chatModel->appendToLastMessage(token);
            }
        }
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

QJsonArray OllamaClient::buildMessages() const
{
    QJsonArray arr;

    QJsonObject sys;
    sys[QStringLiteral("role")] = QStringLiteral("system");
    sys[QStringLiteral("content")] = m_systemPrompt;
    arr.append(sys);

    for (const auto &msg : m_chatModel->messages()) {
        QJsonObject obj;
        obj[QStringLiteral("role")] = msg.role;
        obj[QStringLiteral("content")] = msg.content;
        arr.append(obj);
    }

    return arr;
}
