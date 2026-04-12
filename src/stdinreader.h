#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QTextStream>

class ChatModel;

class StdinReader : public QObject {
    Q_OBJECT

public:
    explicit StdinReader(ChatModel *chatModel, QObject *parent = nullptr);

private slots:
    void onReadyRead();
    void onModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                            const QList<int> &roles);

private:
    void writeJsonLine(const QJsonObject &obj);

    ChatModel *m_chatModel;
    QSocketNotifier m_notifier;
    QTextStream m_stdout;
    int m_lastPrintedLength = 0;
    int m_lastToolLogLength = 0;
    bool m_jsonMode = false;
    bool m_modeDetected = false;
};
