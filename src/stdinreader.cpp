#include "stdinreader.h"
#include "chatmodel.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
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
                if (!m_jsonMode) {
                    if (m_lastPrintedLength > 0) {
                        m_stdout << '\n';
                        m_stdout.flush();
                    }
                } else {
                    // In JSON mode, emit assistant_done for the previous message
                    // when a new row is inserted (meaning the assistant finished).
                    int prevRow = m_chatModel->rowCount() - 2;
                    if (prevRow >= 0) {
                        const auto &msgs = m_chatModel->messages();
                        if (msgs[prevRow].role == QStringLiteral("assistant")
                            && !msgs[prevRow].content.isEmpty()) {
                            QJsonObject obj;
                            obj[QStringLiteral("type")] = QStringLiteral("assistant_done");
                            obj[QStringLiteral("content")] = msgs[prevRow].content;
                            writeJsonLine(obj);
                        }
                    }
                }
                m_lastPrintedLength = 0;
                m_lastToolLogLength = 0;
            });
}

void StdinReader::onReadyRead()
{
    QTextStream in(stdin);
    QString line = in.readLine();

    if (line.isNull()) {
        // EOF — in JSON mode, emit assistant_done for any in-progress message.
        if (m_jsonMode) {
            int lastRow = m_chatModel->rowCount() - 1;
            if (lastRow >= 0) {
                const auto &msgs = m_chatModel->messages();
                if (msgs[lastRow].role == QStringLiteral("assistant")
                    && !msgs[lastRow].content.isEmpty()
                    && m_lastPrintedLength > 0) {
                    QJsonObject obj;
                    obj[QStringLiteral("type")] = QStringLiteral("assistant_done");
                    obj[QStringLiteral("content")] = msgs[lastRow].content;
                    writeJsonLine(obj);
                }
            }
        }
        QCoreApplication::quit();
        return;
    }

    // Auto-detect mode on first non-empty input.
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return;

    if (!m_modeDetected) {
        m_jsonMode = trimmed.startsWith(QLatin1Char('{'));
        m_modeDetected = true;
    }

    if (m_jsonMode) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
        if (doc.isNull() || !doc.isObject()) {
            // Malformed JSON — ignore.
            return;
        }
        QJsonObject obj = doc.object();
        QString type = obj.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("user_message")) {
            QString content = obj.value(QStringLiteral("content")).toString();
            if (!content.isEmpty())
                m_chatModel->addUserMessage(content);
        }
    } else {
        m_chatModel->addUserMessage(trimmed);
    }
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

    const auto &msgs = m_chatModel->messages();
    const auto &msg = msgs[lastRow];
    if (msg.role != QStringLiteral("assistant"))
        return;

    if (m_jsonMode) {
        // Emit tool_log lines for new tool log content.
        if (msg.toolLog.length() > m_lastToolLogLength) {
            QString delta = msg.toolLog.mid(m_lastToolLogLength);
            // Tool log may contain multiple lines; emit each as a separate JSON line.
            const auto lines = delta.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &l : lines) {
                QJsonObject obj;
                obj[QStringLiteral("type")] = QStringLiteral("tool_log");
                obj[QStringLiteral("text")] = l.trimmed();
                writeJsonLine(obj);
            }
            m_lastToolLogLength = msg.toolLog.length();
        }

        // Emit assistant_chunk for new content.
        if (msg.content.length() > m_lastPrintedLength) {
            QString delta = msg.content.mid(m_lastPrintedLength);
            QJsonObject obj;
            obj[QStringLiteral("type")] = QStringLiteral("assistant_chunk");
            obj[QStringLiteral("content")] = delta;
            writeJsonLine(obj);
            m_lastPrintedLength = msg.content.length();
        }
    } else {
        // Plain-text mode: stream content deltas as before.
        // ContentRole returns content + toolLog combined.
        QString combined = msg.content + msg.toolLog;
        int combinedLength = combined.length();
        if (combinedLength > m_lastPrintedLength) {
            QString delta = combined.mid(m_lastPrintedLength);
            m_stdout << delta;
            m_stdout.flush();
            m_lastPrintedLength = combinedLength;
        }
    }
}

void StdinReader::writeJsonLine(const QJsonObject &obj)
{
    m_stdout << QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact));
    m_stdout << '\n';
    m_stdout.flush();
}
