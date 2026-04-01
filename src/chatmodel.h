#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct ChatMessage {
    QString role;    // "user" or "assistant"
    QString content;
};

class ChatModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        RoleRole = Qt::UserRole + 1,
        ContentRole,
    };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addUserMessage(const QString &text);
    void addAssistantMessage();
    void appendToLastMessage(const QString &token);
    const QVector<ChatMessage> &messages() const { return m_messages; }

signals:
    void userMessageAdded(const QString &text);

private:
    QVector<ChatMessage> m_messages;
};
