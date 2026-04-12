#include "applauncher.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QProcessEnvironment>

AppLauncher::AppLauncher(const QString &waylandSocket, QObject *parent)
    : QObject(parent)
    , m_waylandSocket(waylandSocket)
{
    // Hardcoded app map. The clock is the only one the gen 1 test relies on.
    m_apps.insert(QStringLiteral("clock"), QStringLiteral("botnik-clock"));
}

bool AppLauncher::launch(const QString &name, QString *error)
{
    auto it = m_apps.constFind(name);
    if (it == m_apps.cend()) {
        if (error)
            *error = QStringLiteral("unknown app: %1").arg(name);
        return false;
    }

    const QString binaryPath =
        QGuiApplication::applicationDirPath() + QLatin1Char('/') + it.value();

    if (!QFileInfo(binaryPath).isExecutable()) {
        if (error)
            *error =
                QStringLiteral("binary not executable: %1").arg(binaryPath);
        return false;
    }

    auto *proc = new QProcess(this);
    proc->setProgram(binaryPath);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), m_waylandSocket);
    env.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_WAYLAND_DISABLE_WINDOWDECORATION"),
               QStringLiteral("1"));
    proc->setProcessEnvironment(env);

    proc->start();
    if (!proc->waitForStarted(500)) {
        if (error)
            *error = QStringLiteral("failed to start %1: %2")
                         .arg(binaryPath, proc->errorString());
        proc->deleteLater();
        return false;
    }
    return true;
}

QStringList AppLauncher::appNames() const
{
    return m_apps.keys();
}

void AppLauncher::shutdown()
{
    for (QObject *child : children()) {
        auto *proc = qobject_cast<QProcess *>(child);
        if (!proc)
            continue;
        if (proc->state() != QProcess::NotRunning) {
            proc->kill();
            proc->waitForFinished(500);
        }
    }
}
