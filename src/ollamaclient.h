#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

class ChatModel;
class ToolHost;

// OllamaClient talks to a local Ollama server via /api/chat. It streams
// assistant text tokens into the chat model, and — if a ToolHost is attached
// — handles tool-call round trips: when a streamed response contains
// `message.tool_calls`, each call is dispatched through the host, results
// are appended to the conversation as role="tool" messages, and a follow-up
// request is posted until the assistant replies without tool calls.
class OllamaClient : public QObject {
    Q_OBJECT

public:
    explicit OllamaClient(ChatModel *model,
                          ToolHost *tools = nullptr,
                          QObject *parent = nullptr);

    void send(const QString &userMessage);

private slots:
    void onReadyRead();
    void onFinished();

private:
    // Seed m_messages for a new user turn from chat history + system prompt.
    void seedMessagesFromHistory();
    // Post m_messages to /api/chat with streaming enabled.
    void postChat();
    // After a round that contained tool_calls: dispatch them, append results
    // to m_messages, and either loop or stop if the round cap is exceeded.
    void handleToolCallsAndContinue();

    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    ChatModel *m_chatModel;
    ToolHost *m_tools;
    QString m_systemPrompt;
    QString m_modelName;

    // Per-turn state.
    QJsonArray m_messages;           // authoritative history for this turn
    QJsonObject m_currentAssistant;  // accumulates content + tool_calls
    bool m_awaitingToolCalls = false;
    int m_toolRounds = 0;
    static constexpr int kMaxToolRounds = 5;
};
