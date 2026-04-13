#include <QGuiApplication>
#include <QFile>
#include <QPainter>
#include <QRasterWindow>
#include <QTimer>

static QString findBatteryPath()
{
    for (const char *name : {"BAT0", "BAT1"}) {
        QString base = QStringLiteral("/sys/class/power_supply/%1").arg(name);
        if (QFile::exists(base + QStringLiteral("/capacity")))
            return base;
    }
    return {};
}

static QString readFileContent(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

static QString batteryText()
{
    QString base = findBatteryPath();
    if (base.isEmpty())
        return QStringLiteral("NO BAT");

    QString capacity = readFileContent(base + QStringLiteral("/capacity"));
    QString status = readFileContent(base + QStringLiteral("/status"));

    QString tag;
    if (status == QStringLiteral("Charging"))
        tag = QStringLiteral("CHG");
    else if (status == QStringLiteral("Discharging"))
        tag = QStringLiteral("DIS");
    else if (status == QStringLiteral("Full"))
        tag = QStringLiteral("FUL");
    else
        tag = QStringLiteral("UNK");

    return QStringLiteral("BAT %1% %2").arg(capacity, tag);
}

class BatteryWindow : public QRasterWindow {
public:
    BatteryWindow()
    {
        setFlags(Qt::FramelessWindowHint);
        resize(300, 40);
        setTitle(QStringLiteral("botnik-battery"));

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() { update(); });
        timer->start(5000);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(0, 0, width(), height(), QColor("#002b36"));

        p.setPen(QColor("#2aa198"));
        p.setFont(QFont("monospace", 14));

        QString text = batteryText();
        p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, text);
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    BatteryWindow window;
    window.show();
    return app.exec();
}
