#include <QGuiApplication>
#include <QPainter>
#include <QProcess>
#include <QRasterWindow>
#include <QTimer>
#include <QFile>
#include <QTextStream>

struct WifiInfo {
    QString iface;
    QString ssid;
    int qualityPercent = -1; // -1 means no wifi
};

static WifiInfo readFromProcWireless()
{
    WifiInfo info;
    QFile f(QStringLiteral("/proc/net/wireless"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return info;

    QTextStream in(&f);
    // Skip the two header lines.
    in.readLine();
    in.readLine();

    QString line = in.readLine().trimmed();
    if (line.isEmpty())
        return info;

    // Format: "iface: status quality level noise ..."
    // e.g.  "wlan0: 0000  70.  -40.  -256 ..."
    int colonIdx = line.indexOf(QLatin1Char(':'));
    if (colonIdx < 0)
        return info;

    info.iface = line.left(colonIdx).trimmed();

    QStringList fields = line.mid(colonIdx + 1).simplified().split(QLatin1Char(' '));
    if (fields.size() < 3)
        return info;

    // Quality field (index 1) — may have a trailing dot.
    QString qualStr = fields.at(1);
    qualStr.remove(QLatin1Char('.'));
    bool ok = false;
    double qual = qualStr.toDouble(&ok);
    if (!ok)
        return info;

    // If quality is negative, it's in dBm — map -50=100% to -90=0%.
    if (qual < 0) {
        double dBm = qual;
        if (dBm >= -50.0)
            info.qualityPercent = 100;
        else if (dBm <= -90.0)
            info.qualityPercent = 0;
        else
            info.qualityPercent = static_cast<int>((dBm + 90.0) / 40.0 * 100.0);
    } else {
        // Quality out of 70.
        info.qualityPercent = static_cast<int>(qual / 70.0 * 100.0);
        if (info.qualityPercent > 100)
            info.qualityPercent = 100;
    }

    return info;
}

static QString readSsidFromIwgetid()
{
    QProcess proc;
    proc.setProgram(QStringLiteral("iwgetid"));
    proc.setArguments({QStringLiteral("-r")});
    proc.start();
    if (!proc.waitForFinished(2000))
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

static WifiInfo readWifiInfo()
{
    WifiInfo info = readFromProcWireless();

    // Try to get the SSID via iwgetid (works regardless of /proc parsing).
    if (!info.iface.isEmpty()) {
        info.ssid = readSsidFromIwgetid();
        return info;
    }

    // Fallback: try iwgetid to detect any wireless interface.
    QString ssid = readSsidFromIwgetid();
    if (ssid.isEmpty())
        return info; // No wifi at all.

    info.ssid = ssid;
    info.iface = QStringLiteral("wlan0");

    // Try iwconfig for signal level.
    QProcess proc;
    proc.setProgram(QStringLiteral("iwconfig"));
    proc.start();
    if (proc.waitForFinished(2000)) {
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        // Look for "Signal level=-XX dBm" or "Link Quality=XX/70"
        int idx = output.indexOf(QStringLiteral("Link Quality="));
        if (idx >= 0) {
            QString sub = output.mid(idx + 13);
            int slashIdx = sub.indexOf(QLatin1Char('/'));
            if (slashIdx > 0) {
                bool ok = false;
                int qual = sub.left(slashIdx).toInt(&ok);
                if (ok)
                    info.qualityPercent = static_cast<int>(qual / 70.0 * 100.0);
            }
        } else {
            idx = output.indexOf(QStringLiteral("Signal level="));
            if (idx >= 0) {
                QString sub = output.mid(idx + 13);
                bool ok = false;
                int dBm = sub.split(QLatin1Char(' ')).first().toInt(&ok);
                if (ok) {
                    if (dBm >= -50)
                        info.qualityPercent = 100;
                    else if (dBm <= -90)
                        info.qualityPercent = 0;
                    else
                        info.qualityPercent = static_cast<int>((dBm + 90.0) / 40.0 * 100.0);
                }
            }
        }
    }

    return info;
}

class WifiWindow : public QRasterWindow {
public:
    WifiWindow()
    {
        setFlags(Qt::FramelessWindowHint);
        resize(300, 40);
        setTitle(QStringLiteral("botnik-wifi"));

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

        WifiInfo info = readWifiInfo();

        QString text;
        if (info.qualityPercent < 0 && info.ssid.isEmpty()) {
            text = QStringLiteral("NO WIFI");
        } else {
            QString qualStr = info.qualityPercent >= 0
                ? QStringLiteral("%1%").arg(info.qualityPercent)
                : QStringLiteral("??%");
            QString ssid = info.ssid.isEmpty()
                ? QStringLiteral("(unknown)")
                : info.ssid;
            text = QStringLiteral("%1 %2 %3").arg(info.iface, ssid, qualStr);
        }

        p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, text);
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    WifiWindow window;
    window.show();
    return app.exec();
}
