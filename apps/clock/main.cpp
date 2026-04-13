#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

static QString readClockFormat()
{
    QString path = QDir::homePath()
                   + QStringLiteral("/.config/botnik/settings.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QStringLiteral("24h");

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QStringLiteral("24h");

    QString fmt = doc.object()
                      .value(QStringLiteral("clock_format"))
                      .toString();
    if (fmt == QStringLiteral("12h"))
        return fmt;
    return QStringLiteral("24h");
}

class ClockWindow : public QRasterWindow {
public:
    ClockWindow()
    {
        setFlags(Qt::FramelessWindowHint);
        resize(300, 40);
        setTitle(QStringLiteral("botnik-clock"));

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() { update(); });
        timer->start(1000);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(0, 0, width(), height(), QColor("#002b36"));

        p.setPen(QColor("#2aa198"));
        p.setFont(QFont("monospace", 14));

        QString fmt = (readClockFormat() == QStringLiteral("12h"))
                          ? QStringLiteral("h:mm AP")
                          : QStringLiteral("HH:mm");
        QString time = QDateTime::currentDateTime().toString(fmt);
        p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, time);
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    ClockWindow window;
    window.show();
    return app.exec();
}
