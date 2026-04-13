#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QRasterWindow>
#include <QTimer>

class VolumeWindow : public QRasterWindow {
public:
    VolumeWindow()
    {
        setFlags(Qt::FramelessWindowHint);
        resize(100, 40);
        setTitle(QStringLiteral("botnik-volume"));

        readVolume();

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            readVolume();
            update();
        });
        timer->start(2000);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(0, 0, width(), height(), QColor("#002b36"));

        const QColor teal("#2aa198");
        const int h = height();
        const int iconSize = h - 8;
        const int iconX = 4;
        const int iconY = 4;

        // Speaker body: rectangle + triangle forming a speaker shape.
        // Body rect on the left, flare (trapezoid) opening to the right.
        const int bodyW = iconSize / 3;
        const int bodyH = iconSize / 2;
        const int bodyX = iconX;
        const int bodyY = iconY + (iconSize - bodyH) / 2;

        const int flareW = iconSize / 3;
        const int flareX = bodyX + bodyW;

        QPainterPath speakerPath;
        // Rectangle part (back of speaker)
        speakerPath.moveTo(bodyX, bodyY);
        speakerPath.lineTo(bodyX + bodyW, bodyY);
        // Flare top
        speakerPath.lineTo(flareX + flareW, iconY);
        // Flare bottom
        speakerPath.lineTo(flareX + flareW, iconY + iconSize);
        // Back to rectangle bottom
        speakerPath.lineTo(bodyX + bodyW, bodyY + bodyH);
        speakerPath.lineTo(bodyX, bodyY + bodyH);
        speakerPath.closeSubpath();

        p.setPen(Qt::NoPen);
        p.setBrush(teal);
        p.drawPath(speakerPath);

        const int waveX = flareX + flareW + 4;
        const int waveCY = iconY + iconSize / 2;

        if (m_muted) {
            // Draw X over speaker
            p.setPen(QPen(teal, 2));
            const int xOff = waveX + 2;
            const int xSize = 8;
            p.drawLine(xOff, waveCY - xSize / 2, xOff + xSize, waveCY + xSize / 2);
            p.drawLine(xOff, waveCY + xSize / 2, xOff + xSize, waveCY - xSize / 2);
        } else {
            // Draw sound wave arcs based on volume level
            int arcs = m_volume <= 33 ? 1 : (m_volume <= 66 ? 2 : 3);
            p.setPen(QPen(teal, 1.5));
            p.setBrush(Qt::NoBrush);
            for (int i = 0; i < arcs; ++i) {
                int r = 5 + i * 5;
                QRect arcRect(waveX - r, waveCY - r, r * 2, r * 2);
                p.drawArc(arcRect, -45 * 16, 90 * 16);
            }
        }

        // Percentage text to the right of the icon
        p.setPen(teal);
        p.setFont(QFont("monospace", 10));
        const int textX = waveX + 20;
        QString pctText = m_muted ? QStringLiteral("M")
                                  : QStringLiteral("%1%").arg(m_volume);
        p.drawText(QRect(textX, 0, width() - textX, h), Qt::AlignVCenter, pctText);
    }

private:
    void readVolume()
    {
        // Read mute state.
        {
            QProcess proc;
            proc.start(QStringLiteral("pactl"),
                       {QStringLiteral("get-sink-mute"),
                        QStringLiteral("@DEFAULT_SINK@")});
            proc.waitForFinished(1000);
            QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            m_muted = out.contains(QStringLiteral("yes"), Qt::CaseInsensitive);
        }

        // Read volume level.
        {
            QProcess proc;
            proc.start(QStringLiteral("pactl"),
                       {QStringLiteral("get-sink-volume"),
                        QStringLiteral("@DEFAULT_SINK@")});
            proc.waitForFinished(1000);
            QString out = QString::fromUtf8(proc.readAllStandardOutput());
            // Output contains something like "Volume: front-left: 48000 /  73% / ..."
            // Extract the first percentage.
            int idx = out.indexOf(QLatin1Char('%'));
            if (idx > 0) {
                int start = idx - 1;
                while (start > 0 && out.at(start - 1).isDigit())
                    --start;
                m_volume = out.mid(start, idx - start).toInt();
            }
        }
    }

    int m_volume = 0;
    bool m_muted = false;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    VolumeWindow window;
    window.show();
    return app.exec();
}
