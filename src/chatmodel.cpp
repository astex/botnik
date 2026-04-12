#include "chatmodel.h"

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_messages.size();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.size())
        return {};

    const auto &msg = m_messages.at(index.row());
    switch (role) {
    case RoleRole:
        return msg.role;
    case ContentRole:
        return msg.content + msg.toolLog;
    default:
        return {};
    }
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    return {
        {RoleRole, "role"},
        {ContentRole, "content"},
    };
}

void ChatModel::addUserMessage(const QString &text)
{
    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append({QStringLiteral("user"), text});
    endInsertRows();
    emit userMessageAdded(text);
}

void ChatModel::addAssistantMessage()
{
    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append({QStringLiteral("assistant"), QString()});
    endInsertRows();
}

void ChatModel::appendToLastMessage(const QString &token)
{
    if (m_messages.isEmpty())
        return;

    m_messages.last().content += token;
    QModelIndex idx = index(m_messages.size() - 1);
    emit dataChanged(idx, idx, {ContentRole});
}

void ChatModel::appendToolLog(const QString &text)
{
    if (m_messages.isEmpty())
        return;

    m_messages.last().toolLog += text;
    QModelIndex idx = index(m_messages.size() - 1);
    emit dataChanged(idx, idx, {ContentRole});
}
