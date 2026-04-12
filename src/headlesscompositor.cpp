#include "headlesscompositor.h"
#include <QWaylandSurface>

HeadlessCompositor::HeadlessCompositor(QObject *parent)
    : QWaylandCompositor(parent)
{
    // Disable hardware integration — no EGL display in offscreen mode.
    setUseHardwareIntegrationExtension(false);

    // No QWaylandOutput — nothing to render to.
    m_xdgShell = new QWaylandXdgShell(this);
    connect(m_xdgShell, &QWaylandXdgShell::toplevelCreated,
            this, &HeadlessCompositor::onToplevelCreated);

    create();
}

void HeadlessCompositor::onToplevelCreated(QWaylandXdgToplevel *toplevel,
                                           QWaylandXdgSurface *xdgSurface)
{
    toplevel->sendConfigure({0, 0}, QList<QWaylandXdgToplevel::State>());

    m_workspaceModel.addWorkspace(xdgSurface, toplevel);

    connect(xdgSurface->surface(), &QWaylandSurface::surfaceDestroyed,
            this, [this, xdgSurface]() {
                m_workspaceModel.removeBySurface(xdgSurface);
            });
}
