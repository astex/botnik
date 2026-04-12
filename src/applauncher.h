#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

// AppLauncher spawns hardcoded binaries as Wayland clients of the botnik
// compositor. It is constructed with the compositor's WAYLAND_DISPLAY socket
// name, which every launched process inherits via environment. Spawned
// QProcess children are parented to the launcher so they clean up when it is
// destroyed. An explicit shutdown sweep kills any still-running children.
class AppLauncher : public QObject {
    Q_OBJECT

public:
    explicit AppLauncher(const QString &waylandSocket,
                         QObject *parent = nullptr);

    // Launches the named app in its own process. Returns true on success.
    // On failure, sets *error if non-null.
    bool launch(const QString &name, QString *error);

    // Names of the apps this launcher knows how to start.
    QStringList appNames() const;

    // Synchronously kill all still-running child processes. Called from the
    // app's aboutToQuit hook in main.cpp.
    void shutdown();

private:
    QHash<QString, QString> m_apps; // logical name -> binary filename
    QString m_waylandSocket;
};
