#pragma once

#include "compositor.h"

#include <QWaylandCompositor>
#include <QWaylandXdgShell>

// Minimal compositor for headless mode — no QWaylandOutput, no window.
// Accepts Wayland client connections and handles xdg toplevel creation,
// but renders nothing. Maintains a WorkspaceModel so tools work.
class HeadlessCompositor : public QWaylandCompositor {
    Q_OBJECT

public:
    explicit HeadlessCompositor(const QString &socketName = QString(),
                               QObject *parent = nullptr);

    WorkspaceModel *workspaceModel() { return &m_workspaceModel; }

private slots:
    void onToplevelCreated(QWaylandXdgToplevel *toplevel,
                           QWaylandXdgSurface *xdgSurface);

private:
    QWaylandXdgShell *m_xdgShell = nullptr;
    WorkspaceModel m_workspaceModel;
};
