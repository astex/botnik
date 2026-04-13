#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

static QString settingsPath()
{
    return QDir::homePath()
           + QStringLiteral("/.config/botnik/settings.json");
}

static QJsonObject readSettings()
{
    QFile f(settingsPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
}

static void writeSettings(const QJsonObject &obj)
{
    QString path = settingsPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

static QString currentClockFormat()
{
    QString fmt = readSettings()
                      .value(QStringLiteral("clock_format"))
                      .toString();
    if (fmt == QStringLiteral("12h"))
        return fmt;
    return QStringLiteral("24h");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    auto *window = new QWidget;
    window->setWindowTitle(QStringLiteral("Settings"));
    window->setFixedSize(280, 120);

    // Solarized dark theme
    window->setStyleSheet(QStringLiteral(
        "QWidget { background-color: #002b36; color: #839496; }"
        "QLabel { color: #93a1a1; font-size: 13px; }"
        "QPushButton {"
        "  background-color: #073642; color: #2aa198;"
        "  border: 1px solid #586e75; border-radius: 4px;"
        "  padding: 6px 16px; font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #586e75; }"));

    auto *layout = new QVBoxLayout(window);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *heading = new QLabel(QStringLiteral("Clock Format"));
    QFont headingFont = heading->font();
    headingFont.setPointSize(15);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    heading->setStyleSheet(QStringLiteral("color: #eee8d5;"));
    layout->addWidget(heading);

    auto *row = new QHBoxLayout;
    auto *label = new QLabel;
    auto *button = new QPushButton;

    auto updateUI = [label, button]() {
        QString fmt = currentClockFormat();
        label->setText(QStringLiteral("Current: ") + fmt);
        QString other = (fmt == QStringLiteral("12h"))
                            ? QStringLiteral("24h")
                            : QStringLiteral("12h");
        button->setText(QStringLiteral("Switch to ") + other);
    };
    updateUI();

    QObject::connect(button, &QPushButton::clicked, [updateUI]() {
        QJsonObject obj = readSettings();
        QString current = obj.value(QStringLiteral("clock_format"))
                              .toString(QStringLiteral("24h"));
        QString next = (current == QStringLiteral("12h"))
                           ? QStringLiteral("24h")
                           : QStringLiteral("12h");
        obj[QStringLiteral("clock_format")] = next;
        writeSettings(obj);
        updateUI();
    });

    row->addWidget(label);
    row->addStretch();
    row->addWidget(button);
    layout->addLayout(row);
    layout->addStretch();

    window->show();
    return app.exec();
}
