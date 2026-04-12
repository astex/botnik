#pragma once

#include <QWaylandCompositor>
#include <QWaylandXdgShell>

// Minimal compositor for headless mode — no QWaylandOutput, no window.
// Accepts Wayland client connections and handles xdg toplevel creation,
// but renders nothing.
class HeadlessCompositor : public QWaylandCompositor {
    Q_OBJECT

public:
    explicit HeadlessCompositor(QObject *parent = nullptr);

private slots:
    void onToplevelCreated(QWaylandXdgToplevel *toplevel,
                           QWaylandXdgSurface *xdgSurface);

private:
    QWaylandXdgShell *m_xdgShell = nullptr;
};
