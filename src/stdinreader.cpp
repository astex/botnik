#include "stdinreader.h"
#include "chatmodel.h"

#include <QCoreApplication>
#include <QTextStream>
#include <cstdio>

StdinReader::StdinReader(ChatModel *chatModel, QObject *parent)
    : QObject(parent)
    , m_chatModel(chatModel)
    , m_notifier(fileno(stdin), QSocketNotifier::Read, this)
    , m_stdout(stdout)
{
    connect(&m_notifier, &QSocketNotifier::activated,
            this, &StdinReader::onReadyRead);
    connect(m_chatModel, &QAbstractItemModel::dataChanged,
            this, &StdinReader::onModelDataChanged);
    connect(m_chatModel, &QAbstractItemModel::rowsInserted,
            this, [this]() {
                if (m_lastPrintedLength > 0) {
                    m_stdout << '\n';
                    m_stdout.flush();
                }
                m_lastPrintedLength = 0;
            });
}

void StdinReader::onReadyRead()
{
    QTextStream in(stdin);
    QString line = in.readLine();

    if (line.isNull()) {
        // EOF — clean exit
        QCoreApplication::quit();
        return;
    }

    line = line.trimmed();
    if (line.isEmpty())
        return;

    m_chatModel->addUserMessage(line);
}

void StdinReader::onModelDataChanged(const QModelIndex &topLeft,
                                     const QModelIndex &bottomRight,
                                     const QList<int> &roles)
{
    Q_UNUSED(bottomRight);
    Q_UNUSED(roles);

    int lastRow = m_chatModel->rowCount() - 1;
    if (lastRow < 0 || topLeft.row() != lastRow)
        return;

    QModelIndex idx = m_chatModel->index(lastRow);
    QString role = idx.data(ChatModel::RoleRole).toString();
    if (role != QStringLiteral("assistant"))
        return;

    QString content = idx.data(ChatModel::ContentRole).toString();
    if (content.length() > m_lastPrintedLength) {
        QString delta = content.mid(m_lastPrintedLength);
        m_stdout << delta;
        m_stdout.flush();
        m_lastPrintedLength = content.length();
    }
}
