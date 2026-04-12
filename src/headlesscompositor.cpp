#include "headlesscompositor.h"

HeadlessCompositor::HeadlessCompositor(const QString &socketName,
                                       QObject *parent)
    : QWaylandCompositor(parent)
{
    // Disable hardware integration — no EGL display in offscreen mode.
    setUseHardwareIntegrationExtension(false);

    if (!socketName.isEmpty())
        setSocketName(socketName.toLocal8Bit());

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
    Q_UNUSED(xdgSurface);
}
