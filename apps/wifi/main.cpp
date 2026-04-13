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
        resize(150, 40);
        setTitle(QStringLiteral("botnik-wifi"));

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() { update(); });
        timer->start(5000);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(0, 0, width(), height(), QColor("#002b36"));

        const QColor teal("#2aa198");
        const QColor dimTeal("#0e4f49");

        WifiInfo info = readWifiInfo();
        bool disconnected = info.qualityPercent < 0 && info.ssid.isEmpty();

        // Determine how many arcs to fill (1-3) based on signal quality.
        int filledArcs = 0;
        if (!disconnected && info.qualityPercent >= 0) {
            if (info.qualityPercent >= 67)
                filledArcs = 3;
            else if (info.qualityPercent >= 34)
                filledArcs = 2;
            else
                filledArcs = 1;
        }

        // Draw WiFi icon in the left portion of the window.
        // Icon is centered in a 36x36 area with 2px left margin.
        const int iconSize = 30;
        const int iconLeft = 4;
        const int iconCenterX = iconLeft + iconSize / 2;
        const int iconBottom = height() - 6;

        // Draw the base dot.
        const int dotRadius = 3;
        p.setPen(Qt::NoPen);
        p.setBrush(disconnected ? dimTeal : teal);
        p.drawEllipse(QPoint(iconCenterX, iconBottom), dotRadius, dotRadius);

        // Draw 3 concentric quarter-circle arcs radiating upward.
        // QPainter::drawArc uses 1/16th degree units.
        // We draw arcs from 45 degrees to 135 degrees (a 90-degree span).
        const int startAngle = 45 * 16;
        const int spanAngle = 90 * 16;
        const int arcRadii[] = {10, 17, 24};

        for (int i = 0; i < 3; ++i) {
            int r = arcRadii[i];
            QRect arcRect(iconCenterX - r, iconBottom - r, r * 2, r * 2);

            bool active = !disconnected && (i + 1) <= filledArcs;
            QPen arcPen(active ? teal : dimTeal, 2.5);
            p.setPen(arcPen);
            p.setBrush(Qt::NoBrush);
            p.drawArc(arcRect, startAngle, spanAngle);
        }

        // Draw an X over the icon if disconnected.
        if (disconnected) {
            QPen xPen(QColor("#dc322f"), 2);
            p.setPen(xPen);
            int xSize = 6;
            int xCenterY = iconBottom - iconSize / 2;
            p.drawLine(iconCenterX - xSize, xCenterY - xSize,
                       iconCenterX + xSize, xCenterY + xSize);
            p.drawLine(iconCenterX - xSize, xCenterY + xSize,
                       iconCenterX + xSize, xCenterY - xSize);
        }

        // Draw SSID text to the right of the icon.
        p.setPen(teal);
        p.setFont(QFont("monospace", 10));
        QString label = disconnected
            ? QStringLiteral("No WiFi")
            : (info.ssid.isEmpty() ? QStringLiteral("(unknown)") : info.ssid);
        int textLeft = iconLeft + iconSize + 6;
        QRect textRect(textLeft, 0, width() - textLeft - 2, height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, label);
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    WifiWindow window;
    window.show();
    return app.exec();
}
