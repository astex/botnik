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

struct BatteryState {
    int percent = -1;   // -1 means no battery
    bool charging = false;
};

static BatteryState readBattery()
{
    QString base = findBatteryPath();
    if (base.isEmpty())
        return {};

    BatteryState s;
    s.percent = readFileContent(base + QStringLiteral("/capacity")).toInt();
    QString status = readFileContent(base + QStringLiteral("/status"));
    s.charging = (status == QStringLiteral("Charging"));
    return s;
}

class BatteryWindow : public QRasterWindow {
public:
    BatteryWindow()
    {
        setFlags(Qt::FramelessWindowHint);
        resize(120, 40);
        setTitle(QStringLiteral("botnik-battery"));

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() { update(); });
        timer->start(5000);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(0, 0, width(), height(), QColor("#002b36"));

        BatteryState bat = readBattery();
        const QColor teal("#2aa198");

        if (bat.percent < 0) {
            p.setPen(teal);
            p.setFont(QFont("monospace", 12));
            p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, QStringLiteral("NO BAT"));
            return;
        }

        // Battery body: horizontal rectangle with nub on right
        const int bodyW = 36, bodyH = 18;
        const int nubW = 4, nubH = 8;
        const int bodyX = 8, bodyY = (height() - bodyH) / 2;

        // Outline
        p.setPen(QPen(teal, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(bodyX, bodyY, bodyW, bodyH, 2, 2);

        // Nub (positive terminal)
        p.fillRect(bodyX + bodyW, bodyY + (bodyH - nubH) / 2, nubW, nubH, teal);

        // Fill interior proportional to charge
        const int inset = 3;
        int fillMaxW = bodyW - inset * 2;
        int fillW = fillMaxW * qBound(0, bat.percent, 100) / 100;

        QColor fillColor;
        if (bat.percent > 50)
            fillColor = QColor("#859900");
        else if (bat.percent >= 20)
            fillColor = QColor("#b58900");
        else
            fillColor = QColor("#dc322f");

        p.fillRect(bodyX + inset, bodyY + inset, fillW, bodyH - inset * 2, fillColor);

        // Charging indicator
        if (bat.charging) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#eee8d5"));
            // Small lightning bolt centered on battery body
            int cx = bodyX + bodyW / 2;
            int cy = bodyY + bodyH / 2;
            QPolygon bolt;
            bolt << QPoint(cx + 1, cy - 6)
                 << QPoint(cx - 2, cy + 1)
                 << QPoint(cx + 0, cy + 1)
                 << QPoint(cx - 1, cy + 6)
                 << QPoint(cx + 2, cy - 1)
                 << QPoint(cx + 0, cy - 1);
            p.drawPolygon(bolt);
        }

        // Percentage text to the right of the icon
        p.setPen(teal);
        p.setFont(QFont("monospace", 12));
        int textX = bodyX + bodyW + nubW + 6;
        p.drawText(QRect(textX, 0, width() - textX, height()), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("%1%").arg(bat.percent));
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    BatteryWindow window;
    window.show();
    return app.exec();
}
