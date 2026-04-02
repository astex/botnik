#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>
#include <QTimer>
#include <QDateTime>

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

        QString time = QDateTime::currentDateTime()
                           .toString(QStringLiteral("yyyy-MM-dd hh:mm AP"));
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
