#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>

class ChatModel;

class OllamaClient : public QObject {
    Q_OBJECT

public:
    explicit OllamaClient(ChatModel *model, QObject *parent = nullptr);

    void send(const QString &userMessage);

private slots:
    void onReadyRead();
    void onFinished();

private:
    QJsonArray buildMessages() const;

    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    ChatModel *m_chatModel;
    QString m_systemPrompt;
    QString m_modelName;
};
