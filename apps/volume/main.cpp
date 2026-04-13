#include <QGuiApplication>
#include <QPainter>
#include <QProcess>
#include <QRasterWindow>
#include <QTimer>

class VolumeWindow : public QRasterWindow {
public:
    VolumeWindow()
    {
        setFlags(Qt::FramelessWindowHint);
        resize(300, 40);
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
        p.fillRect(0, 0, width(), height(), QColor("#002b36"));

        p.setPen(QColor("#2aa198"));
        p.setFont(QFont("monospace", 14));

        QString text = m_muted ? QStringLiteral("MUTE")
                               : QStringLiteral("VOL %1%").arg(m_volume);
        p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, text);
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
