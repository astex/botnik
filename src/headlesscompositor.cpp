#include "headlesscompositor.h"
#include <QWaylandSurface>

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

    m_workspaceModel.addWorkspace(xdgSurface, toplevel);

    // Auto-pin the clock to the sidebar so the default view is agent-only.
    if (toplevel) {
        int wsId = m_workspaceModel.workspaceAt(m_workspaceModel.count() - 1).id;
        auto tryAutoPin = [this, toplevel, wsId]() {
            if (toplevel->title() == QStringLiteral("botnik-clock"))
                m_workspaceModel.pinToSidebar(wsId);
        };
        tryAutoPin();
        connect(toplevel, &QWaylandXdgToplevel::titleChanged,
                this, tryAutoPin);
    }

    connect(xdgSurface->surface(), &QWaylandSurface::surfaceDestroyed,
            this, [this, xdgSurface]() {
                m_workspaceModel.removeBySurface(xdgSurface);
            });
}
